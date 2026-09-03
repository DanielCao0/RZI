/*
 * RAK4631 + usp_zephyr LoRaWAN example (app/, not usp_zephyr/samples).
 * OTAA join, then a 4-byte counter every 60 s on port 1.
 *
 * Fill DevEUI / JoinEUI / keys / region in boards/rak4631_nrf52840.overlay.
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#include <smtc_modem_api.h>
#include <smtc_modem_utilities.h>
#include <smtc_zephyr_usp_api.h>
#include <smtc_sw_platform_helper.h>

LOG_MODULE_REGISTER(usp, LOG_LEVEL_INF);

#define STACK_ID 0
#define UPLINK_PERIOD_S 60
#define UPLINK_PORT 1

#define DT_MODEM_REGION(region) DT_CAT(SMTC_MODEM_REGION_, region)
#define MODEM_REGION DT_MODEM_REGION(DT_STRING_UNQUOTED(DT_PATH(zephyr_user), user_lorawan_region))

static const uint8_t user_dev_eui[8] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_device_eui);
static const uint8_t user_join_eui[8] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_join_eui);
static const uint8_t user_gen_app_key[16] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_gen_app_key);
static const uint8_t user_app_key[16] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_app_key);

static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static uint32_t uplink_counter;

static void check_rc(const char *what, smtc_modem_return_code_t rc)
{
	if (rc != SMTC_MODEM_RC_OK) {
		LOG_ERR("%s failed: %d", what, rc);
	}
}

static void send_counter(void)
{
	uint8_t buff[4] = {
		(uplink_counter >> 24) & 0xFF,
		(uplink_counter >> 16) & 0xFF,
		(uplink_counter >> 8) & 0xFF,
		uplink_counter & 0xFF,
	};

	smtc_modem_return_code_t rc =
		smtc_modem_request_uplink(STACK_ID, UPLINK_PORT, false, buff, sizeof(buff));
	if (rc != SMTC_MODEM_RC_OK) {
		LOG_ERR("uplink failed: %d", rc);
		return;
	}
	if (gpio_is_ready_dt(&led_green)) {
		gpio_pin_toggle_dt(&led_green);
	}
	LOG_INF("uplink #%u on port %u", uplink_counter, UPLINK_PORT);
	uplink_counter++;
}

static void modem_event_callback(void)
{
	smtc_modem_event_t event = {0};
	uint8_t pending;

	do {
		check_rc("get_event", smtc_modem_get_event(&event, &pending));

		switch (event.event_type) {
		case SMTC_MODEM_EVENT_RESET:
			LOG_INF("RESET: set keys, region, join");
			check_rc("set_deveui", smtc_modem_set_deveui(STACK_ID, user_dev_eui));
			check_rc("set_joineui", smtc_modem_set_joineui(STACK_ID, user_join_eui));
			check_rc("set_appkey", smtc_modem_set_appkey(STACK_ID, user_gen_app_key));
			check_rc("set_nwkkey", smtc_modem_set_nwkkey(STACK_ID, user_app_key));
			check_rc("set_region", smtc_modem_set_region(STACK_ID, MODEM_REGION));
			/* 关掉 LoRaWAN Join 占空比退避（首小时约 toa/10，SF10 大约几十秒一次）。
			 * 调试用；量产应关掉，按规范退避。 */
			check_rc("join_dc_bypass",
				 smtc_modem_set_join_duty_cycle_backoff_bypass(STACK_ID, true));
			check_rc("join", smtc_modem_join_network(STACK_ID));
			break;

		case SMTC_MODEM_EVENT_JOINED:
			LOG_INF("JOINED");
			if (gpio_is_ready_dt(&led_blue)) {
				gpio_pin_set_dt(&led_blue, 1);
			}
			send_counter();
			check_rc("alarm", smtc_modem_alarm_start_timer(UPLINK_PERIOD_S));
			break;

		case SMTC_MODEM_EVENT_JOINFAIL:
			LOG_WRN("JOINFAIL (check keys / region / gateway)");
			break;

		case SMTC_MODEM_EVENT_ALARM:
			send_counter();
			check_rc("alarm", smtc_modem_alarm_start_timer(UPLINK_PERIOD_S));
			break;

		case SMTC_MODEM_EVENT_TXDONE:
			LOG_INF("TXDONE");
			break;

		case SMTC_MODEM_EVENT_DOWNDATA: {
			uint8_t payload[SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH];
			uint8_t size = 0;
			uint8_t remaining = 0;
			smtc_modem_dl_metadata_t meta = {0};

			check_rc("downlink",
				 smtc_modem_get_downlink_data(payload, &size, &meta, &remaining));
			LOG_INF("DOWNLINK port %u, %u bytes", meta.fport, size);
			break;
		}

		default:
			LOG_DBG("event %u", event.event_type);
			break;
		}
	} while (pending > 0);
}

static void wait_usb_console(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;
	int64_t deadline = k_uptime_get() + 10000;

	if (!device_is_ready(dev)) {
		return;
	}

	while (dtr == 0 && k_uptime_get() < deadline) {
		(void)uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		k_sleep(K_MSEC(100));
	}
}

int main(void)
{
	wait_usb_console();

	LOG_INF("rzi USP LoRaWAN on %s", CONFIG_BOARD_TARGET);
	LOG_INF("DevEUI %02X%02X%02X%02X%02X%02X%02X%02X", user_dev_eui[0], user_dev_eui[1],
		user_dev_eui[2], user_dev_eui[3], user_dev_eui[4], user_dev_eui[5], user_dev_eui[6],
		user_dev_eui[7]);

	if (gpio_is_ready_dt(&led_blue)) {
		gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
	}
	if (gpio_is_ready_dt(&led_green)) {
		gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
	}

	SMTC_SW_PLATFORM_INIT();
	SMTC_SW_PLATFORM_VOID(smtc_rac_init());
	SMTC_SW_PLATFORM_VOID(smtc_modem_init(&modem_event_callback));

	LOG_INF("joining (edit keys in boards/rak4631_nrf52840.overlay)");

	k_sleep(K_FOREVER);
	return 0;
}
