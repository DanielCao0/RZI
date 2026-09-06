// SPDX-License-Identifier: Apache-2.0
/**
 * @file
 * @brief Basic RAK RUI3-compatible AT command service.
 *
 * Command behaviors, status strings and asynchronous events follow the RAK
 * RUI3 AT Command Manual. Only the basic OTAA Class A subset is
 * implemented; unsupported parameters are rejected with AT_PARAM_ERROR.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util.h>

#if defined(CONFIG_RZI_AT_NVM)
#include <zephyr/settings/settings.h>
#endif

#include <rzi/at.h>
#include <rzi/lorawan.h>

#define AT_THREAD_STACK_SIZE 2048
#define AT_THREAD_PRIORITY K_PRIO_PREEMPT(14)
#define AT_POLL_INTERVAL_MS 20

/* Longest input: AT+SEND=<port>:<484 hex digits>. */
#define AT_LINE_MAX 600
/* Longest output: downlink event with a maximum size payload. */
#define AT_TX_MAX 640

#define AT_VERSION_STRING "RZI_0.1.0_" CONFIG_BOARD
#define AT_STATUS_OK "OK"

/* Optional devicetree defaults, same contract as the class_a sample. */
#define USER_NODE DT_PATH(zephyr_user)
#if DT_NODE_EXISTS(USER_NODE)
#define AT_HAS_DT_DEFAULTS 1
#define REGION_ENUM(name) DT_CAT(RZI_LORAWAN_REGION_, name)
#define AT_DT_REGION REGION_ENUM(DT_STRING_UNQUOTED(USER_NODE, user_lorawan_region))
#endif

enum at_op {
	AT_OP_RUN,  /* AT+XXX   */
	AT_OP_DESC, /* AT+XXX?  */
	AT_OP_GET,  /* AT+XXX=? */
	AT_OP_SET,  /* AT+XXX=v */
};

struct at_cmd {
	const char *name;
	void (*handler)(enum at_op op, const char *arg);
};

static const struct device *at_uart;
static bool lw_initialized;
static bool join_pending;

static uint8_t at_dev_eui[8];
static uint8_t at_join_eui[8];
static uint8_t at_app_key[16];
static enum rzi_lorawan_region at_region = RZI_LORAWAN_REGION_EU_868;
static bool at_cfm;
static bool at_cfs;

static struct {
	bool valid;
	uint8_t port;
	uint8_t size;
	uint8_t data[RZI_LORAWAN_MAX_PAYLOAD];
} last_downlink;

RING_BUF_DECLARE(at_rx_rb, 1024);
static char at_line[AT_LINE_MAX];
static size_t at_line_len;
static bool at_line_overflow;
static char at_tx[AT_TX_MAX];

static K_THREAD_STACK_DEFINE(at_stack, AT_THREAD_STACK_SIZE);
static struct k_thread at_thread;

/* ------------------------------------------------------------------ */
/* UART output                                                        */
/* ------------------------------------------------------------------ */

static void at_write(const char *s)
{
	while (*s != '\0') {
		uart_poll_out(at_uart, (uint8_t)*s++);
	}
}

/* RUI3 status reply: "\r\n<status>\r\n". */
static void at_status(const char *status)
{
	at_write("\r\n");
	at_write(status);
	at_write("\r\n");
}

/* RUI3 value reply: "\r\n<value>\r\nOK\r\n". */
static void at_value(const char *fmt, ...)
{
	va_list ap;
	int len;

	at_write("\r\n");
	va_start(ap, fmt);
	len = vsnprintk(at_tx, sizeof(at_tx), fmt, ap);
	va_end(ap);
	if (len > 0) {
		at_write(at_tx);
	}
	at_write("\r\nOK\r\n");
}

/* RUI3 unsolicited event: "\r\n+EVT:<...>\r\n". */
static void at_event(const char *fmt, ...)
{
	va_list ap;
	int len;

	at_write("\r\n+EVT:");
	va_start(ap, fmt);
	len = vsnprintk(at_tx, sizeof(at_tx), fmt, ap);
	va_end(ap);
	if (len > 0) {
		at_write(at_tx);
	}
	at_write("\r\n");
}

/* ------------------------------------------------------------------ */
/* Hex helpers: RUI3 keys are MSB first, uppercase on output.         */
/* ------------------------------------------------------------------ */

static int hex_nibble(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

static int hex2bin_exact(const char *hex, uint8_t *out, size_t outlen)
{
	size_t len = strlen(hex);

	if (len != outlen * 2) {
		return -EINVAL;
	}
	for (size_t i = 0; i < outlen; i++) {
		int hi = hex_nibble(hex[i * 2]);
		int lo = hex_nibble(hex[i * 2 + 1]);

		if (hi < 0 || lo < 0) {
			return -EINVAL;
		}
		out[i] = (uint8_t)((hi << 4) | lo);
	}
	return 0;
}

static void at_bin2hex(const uint8_t *in, size_t len, char *out)
{
	static const char digits[] = "0123456789ABCDEF";

	for (size_t i = 0; i < len; i++) {
		out[i * 2] = digits[in[i] >> 4];
		out[i * 2 + 1] = digits[in[i] & 0x0f];
	}
	out[len * 2] = '\0';
}

/* ------------------------------------------------------------------ */
/* Region mapping: RUI3 AT+BAND numbers to RZI regions.               */
/* ------------------------------------------------------------------ */

static int band_to_region(int band, enum rzi_lorawan_region *region)
{
	switch (band) {
	case 1: *region = RZI_LORAWAN_REGION_CN_470; return 0;
	case 2: *region = RZI_LORAWAN_REGION_RU_864; return 0;
	case 3: *region = RZI_LORAWAN_REGION_IN_865; return 0;
	case 4: *region = RZI_LORAWAN_REGION_EU_868; return 0;
	case 5: *region = RZI_LORAWAN_REGION_US_915; return 0;
	case 6: *region = RZI_LORAWAN_REGION_AU_915; return 0;
	case 7: *region = RZI_LORAWAN_REGION_KR_920; return 0;
	case 8: *region = RZI_LORAWAN_REGION_AS_923_GRP1; return 0;
	case 9: *region = RZI_LORAWAN_REGION_AS_923_GRP2; return 0;
	case 10: *region = RZI_LORAWAN_REGION_AS_923_GRP3; return 0;
	case 11: *region = RZI_LORAWAN_REGION_AS_923_GRP4; return 0;
	default: return -EINVAL; /* 0 = EU433 and 12 = LA915 are unsupported. */
	}
}

static int region_to_band(enum rzi_lorawan_region region)
{
	switch (region) {
	case RZI_LORAWAN_REGION_CN_470: return 1;
	case RZI_LORAWAN_REGION_RU_864: return 2;
	case RZI_LORAWAN_REGION_IN_865: return 3;
	case RZI_LORAWAN_REGION_EU_868: return 4;
	case RZI_LORAWAN_REGION_US_915: return 5;
	case RZI_LORAWAN_REGION_AU_915: return 6;
	case RZI_LORAWAN_REGION_KR_920: return 7;
	case RZI_LORAWAN_REGION_AS_923_GRP1: return 8;
	case RZI_LORAWAN_REGION_AS_923_GRP2: return 9;
	case RZI_LORAWAN_REGION_AS_923_GRP3: return 10;
	case RZI_LORAWAN_REGION_AS_923_GRP4: return 11;
	default: return 4;
	}
}

/* ------------------------------------------------------------------ */
/* Flash persistence (Zephyr settings on NVS)                         */
/*                                                                    */
/* The storage area is board-defined through devicetree: the standard */
/* "storage_partition" partition, or the zephyr,settings-partition    */
/* chosen node. RZI never hardcodes flash addresses.                  */
/* ------------------------------------------------------------------ */

#if defined(CONFIG_RZI_AT_NVM)

static int at_nvm_read(settings_read_cb read_cb, void *cb_arg, void *dst,
		       size_t len)
{
	return read_cb(cb_arg, dst, len) == (ssize_t)len ? 0 : -EINVAL;
}

static int at_nvm_set(const char *name, size_t len, settings_read_cb read_cb,
		      void *cb_arg)
{
	const char *next;
	uint8_t band;
	bool cfm;

	ARG_UNUSED(len);

	if (settings_name_steq(name, "deveui", &next) && !next) {
		return at_nvm_read(read_cb, cb_arg, at_dev_eui, sizeof(at_dev_eui));
	}
	if (settings_name_steq(name, "joineui", &next) && !next) {
		return at_nvm_read(read_cb, cb_arg, at_join_eui, sizeof(at_join_eui));
	}
	if (settings_name_steq(name, "appkey", &next) && !next) {
		return at_nvm_read(read_cb, cb_arg, at_app_key, sizeof(at_app_key));
	}
	if (settings_name_steq(name, "band", &next) && !next) {
		if (at_nvm_read(read_cb, cb_arg, &band, sizeof(band)) != 0) {
			return -EINVAL;
		}
		return band_to_region(band, &at_region);
	}
	if (settings_name_steq(name, "cfm", &next) && !next) {
		if (at_nvm_read(read_cb, cb_arg, &cfm, sizeof(cfm)) != 0) {
			return -EINVAL;
		}
		at_cfm = cfm;
		return 0;
	}
	return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(rzi_at, "rzi", NULL, at_nvm_set, NULL, NULL);

static void at_nvm_save(const char *key, const void *value, size_t len)
{
	char name[24];

	(void)snprintk(name, sizeof(name), "rzi/%s", key);
	(void)settings_save_one(name, value, len);
}

#else

#define at_nvm_save(key, value, len) do { } while (0)

#endif /* CONFIG_RZI_AT_NVM */

/* ------------------------------------------------------------------ */
/* LoRaWAN service interaction                                        */
/* ------------------------------------------------------------------ */

static bool at_is_joined(void)
{
	bool joined = false;

	if (!lw_initialized) {
		return false;
	}
	return rzi_lorawan_is_joined(&joined) == 0 && joined;
}

static void at_join_start(void)
{
	if (!lw_initialized) {
		struct rzi_lorawan_config config = {
			.region = at_region,
			/* LoRaWAN 1.0.x: AppKey is the LBM network root key. */
		};

		memcpy(config.dev_eui, at_dev_eui, sizeof(config.dev_eui));
		memcpy(config.join_eui, at_join_eui, sizeof(config.join_eui));
		memcpy(config.network_key, at_app_key, sizeof(config.network_key));
		memcpy(config.application_key, at_app_key,
		       sizeof(config.application_key));

		if (rzi_lorawan_init(&config) != 0) {
			at_status("AT_ERROR");
			return;
		}
		lw_initialized = true;
		/* Join when the backend reports READY. */
		join_pending = true;
		at_status(AT_STATUS_OK);
		return;
	}

	if (at_is_joined()) {
		at_status(AT_STATUS_OK);
		at_event("JOINED");
		return;
	}

	int rc = rzi_lorawan_join();

	if (rc == -EBUSY) {
		at_status("AT_BUSY_ERROR");
	} else if (rc != 0) {
		at_status("AT_ERROR");
	} else {
		at_status(AT_STATUS_OK);
	}
}

static void at_join_stop(void)
{
	join_pending = false;
	if (lw_initialized) {
		(void)rzi_lorawan_leave();
	}
	at_status(AT_STATUS_OK);
}

static void handle_lorawan_event(const struct rzi_lorawan_event *event)
{
	switch (event->type) {
	case RZI_LORAWAN_READY:
		if (join_pending) {
			join_pending = false;
			(void)rzi_lorawan_join();
		}
		break;
	case RZI_LORAWAN_JOINED:
		at_event("JOINED");
		break;
	case RZI_LORAWAN_JOIN_FAILED:
		at_event("JOIN_FAILED_RX_TIMEOUT");
		break;
	case RZI_LORAWAN_TX_DONE:
		if (!at_cfm) {
			at_event("TX_DONE");
		} else if (event->tx_result == RZI_LORAWAN_TX_ACKED) {
			at_cfs = true;
			at_event("SEND_CONFIRMED_OK");
		} else if (event->tx_result == RZI_LORAWAN_TX_NOT_SENT) {
			at_cfs = false;
			at_event("SEND_CONFIRMED_FAILED");
		} else {
			at_event("TX_DONE");
		}
		break;
	case RZI_LORAWAN_DOWNLINK: {
		char hex[RZI_LORAWAN_MAX_PAYLOAD * 2 + 1];

		last_downlink.valid = true;
		last_downlink.port = event->downlink.port;
		last_downlink.size = event->downlink.size;
		memcpy(last_downlink.data, event->downlink.data, event->downlink.size);
		at_bin2hex(event->downlink.data, event->downlink.size, hex);
		at_event("RX_1:%d:%d:UNICAST:%u:%s", event->downlink.rssi_dbm,
			 event->downlink.snr_quarter_db / 4, event->downlink.port, hex);
		break;
	}
	case RZI_LORAWAN_ERROR:
		break;
	}
}

/* ------------------------------------------------------------------ */
/* Command handlers                                                   */
/* ------------------------------------------------------------------ */

static void desc_ok(const char *text)
{
	at_value("%s", text);
}

static void handle_ver(enum at_op op, const char *arg)
{
	ARG_UNUSED(arg);

	if (op == AT_OP_DESC) {
		desc_ok("AT+VER: get the version of the firmware");
	} else if (op == AT_OP_GET) {
		at_value("AT+VER=%s", AT_VERSION_STRING);
	} else {
		at_status("AT_ERROR");
	}
}

static void handle_eui(const char *name, const char *desc, const char *nvm_key,
		       uint8_t *value, size_t len, enum at_op op, const char *arg)
{
	char hex[33];

	if (op == AT_OP_DESC) {
		desc_ok(desc);
	} else if (op == AT_OP_GET) {
		at_bin2hex(value, len, hex);
		at_value("%s=%s", name, hex);
	} else if (op == AT_OP_SET) {
		if (hex2bin_exact(arg, value, len) != 0) {
			at_status("AT_PARAM_ERROR");
		} else {
			at_nvm_save(nvm_key, value, len);
			at_status(AT_STATUS_OK);
		}
	} else {
		at_status("AT_ERROR");
	}
}

static void handle_deveui(enum at_op op, const char *arg)
{
	handle_eui("AT+DEVEUI", "AT+DEVEUI: get or set the device EUI (8 bytes in hex)",
		   "deveui", at_dev_eui, sizeof(at_dev_eui), op, arg);
}

static void handle_appeui(enum at_op op, const char *arg)
{
	handle_eui("AT+APPEUI",
		   "AT+APPEUI: get or set the application EUI (8 bytes in hex)",
		   "joineui", at_join_eui, sizeof(at_join_eui), op, arg);
}

static void handle_appkey(enum at_op op, const char *arg)
{
	handle_eui("AT+APPKEY",
		   "AT+APPKEY: get or set the application key (16 bytes in hex)",
		   "appkey", at_app_key, sizeof(at_app_key), op, arg);
}

static void handle_band(enum at_op op, const char *arg)
{
	if (op == AT_OP_DESC) {
		desc_ok("AT+BAND: get or set the active region (0 = EU433, 1 = CN470,"
			" 2 = RU864, 3 = IN865, 4 = EU868, 5 = US915, 6 = AU915,"
			" 7 = KR920, 8 = AS923-1, 9 = AS923-2, 10 = AS923-3,"
			" 11 = AS923-4, 12 = LA915)");
	} else if (op == AT_OP_GET) {
		at_value("AT+BAND=%d", region_to_band(at_region));
	} else if (op == AT_OP_SET) {
		char *end = NULL;
		long band = strtol(arg, &end, 10);
		enum rzi_lorawan_region region;

		if (end == arg || *end != '\0' ||
		    band_to_region((int)band, &region) != 0) {
			at_status("AT_PARAM_ERROR");
		} else {
			uint8_t stored = (uint8_t)band;

			at_region = region;
			at_nvm_save("band", &stored, sizeof(stored));
			at_status(AT_STATUS_OK);
		}
	} else {
		at_status("AT_ERROR");
	}
}

static void handle_njm(enum at_op op, const char *arg)
{
	if (op == AT_OP_DESC) {
		desc_ok("AT+NJM: get or set the network join mode (0 = ABP, 1 = OTAA)");
	} else if (op == AT_OP_GET) {
		at_value("AT+NJM=1");
	} else if (op == AT_OP_SET) {
		/* Only OTAA is implemented; ABP is a planned service. */
		if (strcmp(arg, "1") == 0) {
			at_status(AT_STATUS_OK);
		} else {
			at_status("AT_PARAM_ERROR");
		}
	} else {
		at_status("AT_ERROR");
	}
}

static void handle_njs(enum at_op op, const char *arg)
{
	ARG_UNUSED(arg);

	if (op == AT_OP_DESC) {
		desc_ok("AT+NJS: get the join status (0 = not joined, 1 = joined)");
	} else if (op == AT_OP_GET) {
		at_value("AT+NJS=%d", at_is_joined() ? 1 : 0);
	} else {
		at_status("AT_ERROR");
	}
}

static void handle_class(enum at_op op, const char *arg)
{
	if (op == AT_OP_DESC) {
		desc_ok("AT+CLASS: get or set the device class (A = class A,"
			" B = class B, C = class C)");
	} else if (op == AT_OP_GET) {
		at_value("AT+CLASS=A");
	} else if (op == AT_OP_SET) {
		/* Only Class A is implemented. */
		if (strcmp(arg, "A") == 0) {
			at_status(AT_STATUS_OK);
		} else {
			at_status("AT_PARAM_ERROR");
		}
	} else {
		at_status("AT_ERROR");
	}
}

static void handle_cfm(enum at_op op, const char *arg)
{
	if (op == AT_OP_DESC) {
		desc_ok("AT+CFM: get or set the confirmation mode (0 = OFF, 1 = ON)");
	} else if (op == AT_OP_GET) {
		at_value("AT+CFM=%d", at_cfm ? 1 : 0);
	} else if (op == AT_OP_SET) {
		if (strcmp(arg, "0") == 0) {
			at_cfm = false;
			at_nvm_save("cfm", &at_cfm, sizeof(at_cfm));
			at_status(AT_STATUS_OK);
		} else if (strcmp(arg, "1") == 0) {
			at_cfm = true;
			at_nvm_save("cfm", &at_cfm, sizeof(at_cfm));
			at_status(AT_STATUS_OK);
		} else {
			at_status("AT_PARAM_ERROR");
		}
	} else {
		at_status("AT_ERROR");
	}
}

static void handle_cfs(enum at_op op, const char *arg)
{
	ARG_UNUSED(arg);

	if (op == AT_OP_DESC) {
		desc_ok("AT+CFS: get the confirmation status of the last AT+SEND"
			" (0 = failure, 1 = success)");
	} else if (op == AT_OP_GET) {
		at_value("AT+CFS=%d", at_cfs ? 1 : 0);
	} else {
		at_status("AT_ERROR");
	}
}

static void handle_join(enum at_op op, const char *arg)
{
	if (op == AT_OP_DESC) {
		desc_ok("AT+JOIN: join network");
	} else if (op == AT_OP_GET) {
		at_value("AT+JOIN=1:0:8:0");
	} else if (op == AT_OP_RUN) {
		at_join_start();
	} else if (op == AT_OP_SET) {
		/* Auto-join, interval and attempt parameters are accepted for
		 * RUI3 compatibility but not acted upon yet.
		 */
		if (arg[0] == '1' && (arg[1] == '\0' || arg[1] == ':')) {
			at_join_start();
		} else if (arg[0] == '0' && (arg[1] == '\0' || arg[1] == ':')) {
			at_join_stop();
		} else {
			at_status("AT_PARAM_ERROR");
		}
	}
}

static void handle_send(enum at_op op, const char *arg)
{
	static uint8_t payload[RZI_LORAWAN_MAX_PAYLOAD];
	char *colon;
	char *end = NULL;
	const char *hex;
	long port;
	size_t hex_len;
	int rc;

	if (op == AT_OP_DESC) {
		desc_ok("AT+SEND: send data along with the application port");
		return;
	}
	if (op != AT_OP_SET) {
		at_status("AT_ERROR");
		return;
	}

	colon = strchr(arg, ':');
	if (colon == NULL) {
		at_status("AT_PARAM_ERROR");
		return;
	}
	*colon = '\0';
	hex = colon + 1;

	port = strtol(arg, &end, 10);
	if (end == arg || *end != '\0' || port < 1 || port > 223) {
		at_status("AT_PARAM_ERROR");
		return;
	}

	hex_len = strlen(hex);
	if (hex_len == 0 || hex_len > RZI_LORAWAN_MAX_PAYLOAD * 2) {
		at_status("AT_PARAM_ERROR");
		return;
	}
	if (hex2bin_exact(hex, payload, hex_len / 2) != 0) {
		at_status("AT_PARAM_ERROR");
		return;
	}

	if (!at_is_joined()) {
		at_status("AT_NO_NETWORK_JOINED");
		return;
	}

	rc = rzi_lorawan_send((uint8_t)port, payload, hex_len / 2, at_cfm);
	if (rc == -EBUSY) {
		at_status("AT_BUSY_ERROR");
	} else if (rc == -EINVAL) {
		at_status("AT_PARAM_ERROR");
	} else if (rc != 0) {
		at_status("AT_ERROR");
	} else {
		at_status(AT_STATUS_OK);
	}
}

static void handle_recv(enum at_op op, const char *arg)
{
	char hex[RZI_LORAWAN_MAX_PAYLOAD * 2 + 1];

	ARG_UNUSED(arg);

	if (op == AT_OP_DESC) {
		desc_ok("AT+RECV: print the last received data in hex format");
	} else if (op == AT_OP_GET) {
		if (last_downlink.valid) {
			last_downlink.valid = false;
			at_bin2hex(last_downlink.data, last_downlink.size, hex);
			at_value("AT+RECV=%u:%s", last_downlink.port, hex);
		} else {
			at_value("AT+RECV=");
		}
	} else {
		at_status("AT_ERROR");
	}
}

static const struct at_cmd at_commands[] = {
	{ "VER", handle_ver },
	{ "DEVEUI", handle_deveui },
	{ "APPEUI", handle_appeui },
	{ "APPKEY", handle_appkey },
	{ "BAND", handle_band },
	{ "NJM", handle_njm },
	{ "NJS", handle_njs },
	{ "CLASS", handle_class },
	{ "CFM", handle_cfm },
	{ "CFS", handle_cfs },
	{ "JOIN", handle_join },
	{ "SEND", handle_send },
	{ "RECV", handle_recv },
};

/* ATR: erase the persisted parameters, then reboot so the devicetree
 * defaults apply again.
 */
static void at_factory_reset(void)
{
#if defined(CONFIG_RZI_AT_NVM)
	static const char *const keys[] = {
		"deveui", "joineui", "appkey", "band", "cfm",
	};

	for (size_t i = 0; i < ARRAY_SIZE(keys); i++) {
		char name[24];

		(void)snprintk(name, sizeof(name), "rzi/%s", keys[i]);
		(void)settings_delete(name);
	}
#endif
	at_status(AT_STATUS_OK);
	sys_reboot(SYS_REBOOT_COLD);
}

/* ------------------------------------------------------------------ */
/* Parser                                                             */
/* ------------------------------------------------------------------ */

static void at_execute(char *line)
{
	char *body;
	enum at_op op;
	const char *arg = NULL;

	/* RUI3 accepts mixed case; all parameters of this subset are
	 * numeric or case-insensitive hex, so uppercase the whole line.
	 */
	for (char *p = line; *p != '\0'; p++) {
		if (*p >= 'a' && *p <= 'z') {
			*p -= 'a' - 'A';
		}
	}

	if (strcmp(line, "AT") == 0) {
		at_status(AT_STATUS_OK);
		return;
	}
	if (strcmp(line, "ATZ") == 0) {
		/* ATZ resets the MCU without a reply. */
		sys_reboot(SYS_REBOOT_COLD);
		return;
	}
	if (strcmp(line, "ATZ?") == 0) {
		desc_ok("ATZ: triggers a reset on the MCU.");
		return;
	}
	if (strcmp(line, "ATR") == 0) {
		at_factory_reset();
		return;
	}
	if (strcmp(line, "ATR?") == 0) {
		desc_ok("ATR: restore default parameters");
		return;
	}
	if (strncmp(line, "AT+", 3) != 0) {
		at_status("AT_ERROR");
		return;
	}

	body = line + 3;
	char *eq = strchr(body, '=');
	char *qm = strchr(body, '?');

	if (eq != NULL && eq[1] == '?') {
		op = AT_OP_GET;
		*eq = '\0';
	} else if (eq != NULL) {
		op = AT_OP_SET;
		*eq = '\0';
		arg = eq + 1;
	} else if (qm != NULL && qm[1] == '\0') {
		op = AT_OP_DESC;
		*qm = '\0';
	} else if (eq == NULL && qm == NULL) {
		op = AT_OP_RUN;
	} else {
		at_status("AT_ERROR");
		return;
	}

	for (size_t i = 0; i < ARRAY_SIZE(at_commands); i++) {
		if (strcmp(body, at_commands[i].name) == 0) {
			at_commands[i].handler(op, arg);
			return;
		}
	}
	at_status("AT_ERROR");
}

/* ------------------------------------------------------------------ */
/* UART input                                                         */
/* ------------------------------------------------------------------ */

static void at_uart_isr(const struct device *dev, void *user_data)
{
	uint8_t buf[32];

	ARG_UNUSED(user_data);
	uart_irq_update(dev);
	while (uart_irq_is_pending(dev) && uart_irq_rx_ready(dev)) {
		/* uart_fifo_read must be called until the FIFO is drained. */
		int n = uart_fifo_read(dev, buf, sizeof(buf));

		if (n > 0) {
			(void)ring_buf_put(&at_rx_rb, buf, (uint32_t)n);
		}
		if (n < (int)sizeof(buf)) {
			break;
		}
		uart_irq_update(dev);
	}
}

static void at_rx_byte(uint8_t c)
{
	if (c == '\b' || c == 0x7f) {
		if (at_line_len > 0) {
			at_line_len--;
			at_write("\b \b");
		}
		return;
	}
	if (c == '\r' || c == '\n') {
		if (c == '\r') {
			at_write("\r\n");
		}
		if (at_line_overflow) {
			at_line_overflow = false;
			at_line_len = 0;
			at_status("AT_TEST_PARAM_OVERFLOW");
			return;
		}
		if (at_line_len > 0) {
			at_line[at_line_len] = '\0';
			at_line_len = 0;
			at_execute(at_line);
		}
		return;
	}
	if (c < ' ' || c > '~') {
		return;
	}
	if (at_line_len >= sizeof(at_line) - 1) {
		at_line_overflow = true;
		return;
	}
	at_line[at_line_len++] = (char)c;
	uart_poll_out(at_uart, c);
}

static void at_drain_uart(void)
{
	uint8_t buf[64];
	uint32_t n;

	while ((n = ring_buf_get(&at_rx_rb, buf, sizeof(buf))) > 0) {
		for (uint32_t i = 0; i < n; i++) {
			at_rx_byte(buf[i]);
		}
	}
}

/* ------------------------------------------------------------------ */
/* Service thread                                                     */
/* ------------------------------------------------------------------ */

static void at_thread_fn(void *p1, void *p2, void *p3)
{
	struct rzi_lorawan_event event;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (;;) {
		/* Before the first AT+JOIN the service is not initialized
		 * and rzi_lorawan_get_event would return -EAGAIN at once.
		 */
		if (lw_initialized) {
			if (rzi_lorawan_get_event(&event, AT_POLL_INTERVAL_MS) == 0) {
				handle_lorawan_event(&event);
			}
		} else {
			k_sleep(K_MSEC(AT_POLL_INTERVAL_MS));
		}
		at_drain_uart();
	}
}

int rzi_at_init(const struct device *uart)
{
	static bool started;

	if (uart == NULL || !device_is_ready(uart)) {
		return -ENODEV;
	}
	if (started) {
		return -EALREADY;
	}
	started = true;
	at_uart = uart;

#if defined(AT_HAS_DT_DEFAULTS)
	BUILD_ASSERT(sizeof(at_dev_eui) ==
		     DT_PROP_LEN(USER_NODE, user_lorawan_device_eui));
	BUILD_ASSERT(sizeof(at_join_eui) ==
		     DT_PROP_LEN(USER_NODE, user_lorawan_join_eui));
	BUILD_ASSERT(sizeof(at_app_key) ==
		     DT_PROP_LEN(USER_NODE, user_lorawan_app_key));
	{
		static const uint8_t dev_eui[] = DT_PROP(USER_NODE, user_lorawan_device_eui);
		static const uint8_t join_eui[] = DT_PROP(USER_NODE, user_lorawan_join_eui);
		static const uint8_t app_key[] = DT_PROP(USER_NODE, user_lorawan_app_key);

		memcpy(at_dev_eui, dev_eui, sizeof(at_dev_eui));
		memcpy(at_join_eui, join_eui, sizeof(at_join_eui));
		memcpy(at_app_key, app_key, sizeof(at_app_key));
		at_region = AT_DT_REGION;
	}
#endif

#if defined(CONFIG_RZI_AT_NVM)
	/* Stored values override the devicetree defaults. */
	(void)settings_subsys_init();
	(void)settings_load();
#endif

	uart_irq_callback_set(at_uart, at_uart_isr);
	uart_irq_rx_enable(at_uart);

	k_thread_create(&at_thread, at_stack, AT_THREAD_STACK_SIZE,
			at_thread_fn, NULL, NULL, NULL, AT_THREAD_PRIORITY, 0,
			K_NO_WAIT);
	k_thread_name_set(&at_thread, "rzi_at");
	return 0;
}
