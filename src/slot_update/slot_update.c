/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI1 slot update on any Zephyr UART (type 1 → image-1).
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>

#include <rzi/slot_update.h>

#if defined(CONFIG_SOC_SERIES_NRF52X)
#include <hal/nrf_power.h>
#endif

LOG_MODULE_REGISTER(rzi_slot_update, LOG_LEVEL_INF);

#if DT_HAS_CHOSEN(rzi_slot_update_uart)
#define RZI_SLOT_UPDATE_UART_NODE DT_CHOSEN(rzi_slot_update_uart)
#elif DT_NODE_EXISTS(DT_ALIAS(rzi_slot_update_uart))
#define RZI_SLOT_UPDATE_UART_NODE DT_ALIAS(rzi_slot_update_uart)
#elif DT_HAS_CHOSEN(zephyr_console)
#define RZI_SLOT_UPDATE_UART_NODE DT_CHOSEN(zephyr_console)
#else
#error "Set chosen rzi,slot-update-uart (or zephyr,console) to a UART"
#endif

#define UART_IS_CDC DT_NODE_HAS_COMPAT(RZI_SLOT_UPDATE_UART_NODE, zephyr_cdc_acm_uart)

#define RZI1_RDY    "RZI1-RDY\n"
#define RZI1_GO     "RZI1-GO\n"
#define CHUNK_SIZE  256
#define IMAGE_MAGIC 0x96f3b83dU

struct rzi1_hdr {
	char magic[4];
	uint8_t type;
	uint8_t reserved[3];
	uint32_t length;
	uint32_t crc32;
} __packed;

BUILD_ASSERT(sizeof(struct rzi1_hdr) == 16, "RZI1 header must be 16 bytes");

const struct device *rzi_slot_update_uart(void)
{
	return DEVICE_DT_GET(RZI_SLOT_UPDATE_UART_NODE);
}

static void uart_write(const struct device *uart, const void *data, size_t len)
{
	const uint8_t *p = data;

	while (len--) {
		uart_poll_out(uart, *p++);
	}
}

static void uart_puts(const struct device *uart, const char *s)
{
	uart_write(uart, s, strlen(s));
}

static int uart_read_n(const struct device *uart, uint8_t *buf, size_t len, int idle_ms)
{
	size_t got = 0;
	int64_t deadline = k_uptime_get() + idle_ms;

	while (got < len) {
		uint8_t c;

		if (uart_poll_in(uart, &c) == 0) {
			buf[got++] = c;
			deadline = k_uptime_get() + idle_ms;
			continue;
		}
		if (k_uptime_get() > deadline) {
			return -ETIMEDOUT;
		}
		k_sleep(K_MSEC(1));
	}
	return 0;
}

static int wait_host(const struct device *uart)
{
	if (!UART_IS_CDC) {
		return 0;
	}

	while (true) {
		uint32_t dtr = 0;

		if (uart_line_ctrl_get(uart, UART_LINE_CTRL_DTR, &dtr) == 0 && dtr) {
			return 0;
		}
		k_sleep(K_MSEC(50));
	}
}

static int write_slot1(const struct device *uart, uint32_t length, uint32_t expect_crc)
{
	struct flash_img_context ctx;
	uint8_t buf[CHUNK_SIZE];
	uint32_t crc = 0;
	uint32_t off = 0;
	int rc;

	rc = flash_img_init(&ctx);
	if (rc) {
		return rc;
	}

	uart_puts(uart, RZI1_GO);

	while (off < length) {
		size_t n = MIN(sizeof(buf), length - off);
		bool flush = (off + n) >= length;

		rc = uart_read_n(uart, buf, n, 10000);
		if (rc) {
			return rc;
		}

		if (off == 0U && n >= 4U && sys_get_le32(buf) != IMAGE_MAGIC) {
			return -EINVAL;
		}

		crc = crc32_ieee_update(crc, buf, n);
		rc = flash_img_buffered_write(&ctx, buf, n, flush);
		if (rc) {
			return rc;
		}
		off += n;
	}

	if (crc != expect_crc) {
		return -EBADMSG;
	}

	return boot_request_upgrade(BOOT_UPGRADE_PERMANENT);
}

bool rzi_slot_update_requested(void)
{
#if defined(CONFIG_SOC_SERIES_NRF52X)
	uint32_t v = nrf_power_gpregret_get(NRF_POWER, 0);

	if (v == RZI_SLOT_UPDATE_GPREGRET) {
		nrf_power_gpregret_set(NRF_POWER, 0, 0);
		return true;
	}
#endif
	return false;
}

void rzi_slot_update_arm_reboot(void)
{
#if defined(CONFIG_SOC_SERIES_NRF52X)
	nrf_power_gpregret_set(NRF_POWER, 0, RZI_SLOT_UPDATE_GPREGRET);
#endif
	sys_reboot(SYS_REBOOT_COLD);
}

int rzi_slot_update_run(const struct device *uart)
{
	struct rzi1_hdr hdr;
	int64_t deadline;
	int rc;

	if (uart == NULL || !device_is_ready(uart)) {
		return -ENODEV;
	}

	rc = wait_host(uart);
	if (rc) {
		return rc;
	}

	k_sleep(K_MSEC(200));
	deadline = k_uptime_get() + CONFIG_RZI_SLOT_UPDATE_HEADER_TIMEOUT_MS;
	rc = -ETIMEDOUT;
	while (k_uptime_get() < deadline) {
		uart_puts(uart, RZI1_RDY);
		rc = uart_read_n(uart, (uint8_t *)&hdr, sizeof(hdr), 1000);
		if (rc == 0) {
			break;
		}
	}
	if (rc) {
		return rc;
	}

	if (memcmp(hdr.magic, RZI1_MAGIC, 4) != 0) {
		uart_puts(uart, "ERR MAGIC\n");
		return -EINVAL;
	}
	if (hdr.type != RZI1_TYPE_SLOT1) {
		uart_puts(uart, "ERR TYPE\n");
		return -EINVAL;
	}
	if (hdr.length == 0U) {
		uart_puts(uart, "ERR LEN\n");
		return -EINVAL;
	}

	LOG_INF("RZI1 slot1 len %u", hdr.length);
	rc = write_slot1(uart, hdr.length, hdr.crc32);
	if (rc == -EBADMSG) {
		uart_puts(uart, "ERR CRC\n");
		return rc;
	}
	if (rc == -ETIMEDOUT) {
		uart_puts(uart, "ERR TO\n");
		return rc;
	}
	if (rc) {
		uart_puts(uart, "ERR FLASH\n");
		return rc;
	}

	uart_puts(uart, "OK\n");
	k_sleep(K_MSEC(100));
	rzi_slot_update_arm_reboot();
	return 0;
}

#if defined(CONFIG_RZI_SLOT_UPDATE_1200BPS)

static void baud_watch(void *arg1, void *arg2, void *arg3)
{
	const struct device *uart = rzi_slot_update_uart();

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	if (!device_is_ready(uart)) {
		return;
	}

	while (true) {
		uint32_t baud = 0;

		if (uart_line_ctrl_get(uart, UART_LINE_CTRL_BAUD_RATE, &baud) == 0 &&
		    baud == 1200U) {
			rzi_slot_update_arm_reboot();
		}
		k_sleep(K_MSEC(80));
	}
}

K_THREAD_DEFINE(rzi_slot_baud_tid, 512, baud_watch, NULL, NULL, NULL, 14, 0, 0);

#endif
