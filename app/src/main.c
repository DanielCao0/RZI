/*
 * RAK4631 + usp_zephyr LoRaWAN example (app/, not usp_zephyr/samples).
 * OTAA join, then a 4-byte confirmed counter every 5 s on port 1.
 *
 * Fill DevEUI / JoinEUI / keys / region in boards/rak4631_nrf52840.overlay.
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <hal/nrf_power.h>

#include <smtc_modem_api.h>
#include <smtc_modem_utilities.h>
#include <smtc_zephyr_usp_api.h>
#include <smtc_sw_platform_helper.h>

LOG_MODULE_REGISTER(usp, LOG_LEVEL_INF);

#define STACK_ID 0
#define PERIOD_S 5
#define UPLINK_PORT 1

#define DT_MODEM_REGION(region) DT_CAT(SMTC_MODEM_REGION_, region)
#define MODEM_REGION DT_MODEM_REGION(DT_STRING_UNQUOTED(DT_PATH(zephyr_user), user_lorawan_region))

static const uint8_t user_dev_eui[8] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_device_eui);
static const uint8_t user_join_eui[8] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_join_eui);
static const uint8_t user_gen_app_key[16] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_gen_app_key);
static const uint8_t user_app_key[16] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_app_key);

static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

/* P1.05 = Arduino 37。Bernd：RADIO_RXEN + USE_RXEN_ANT_PWR = 天线开关的供电脚，
 * 不是通路选择脚。高电平有效，通电后方向由 SX1262 的 DIO2 自己切换，
 * 所以这里只需要常开、不需要每次 TX/RX 反复动作。 */
#define ANT_SWITCH_PWR_PIN 5
/* P1.07 原理图接到开关控制，官方要求不要初始化，交给 DIO2。 */
#define ANT_SWITCH_CTRL_PIN 7

static uint32_t uplink_counter;

static void led_set(const struct gpio_dt_spec *led, int value)
{
	if (gpio_is_ready_dt(led)) {
		gpio_pin_set_dt(led, value);
	}
}

static void led_green_blink_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	if (gpio_is_ready_dt(&led_green)) {
		gpio_pin_toggle_dt(&led_green);
	}
}

static void led_blue_off_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	led_set(&led_blue, 0);
}

static K_TIMER_DEFINE(led_green_blink, led_green_blink_handler, NULL);
static K_WORK_DELAYABLE_DEFINE(led_blue_off_work, led_blue_off_work_handler);

static void led_blue_off(void)
{
	(void)k_work_cancel_delayable(&led_blue_off_work);
	led_set(&led_blue, 0);
}

/* 入网前绿灯闪；入网后绿灯常亮。蓝灯常灭，只在发送时快闪。 */
static void leds_joining(void)
{
	led_blue_off();
	led_set(&led_green, 1);
	k_timer_start(&led_green_blink, K_MSEC(500), K_MSEC(500));
}

static void leds_joined(void)
{
	k_timer_stop(&led_green_blink);
	led_set(&led_green, 1);
	led_blue_off();
}

/* 蓝灯常灭；每次上行快闪一下。关灯走 workqueue，避免 timer ISR 里 GPIO 没落下去。 */
static void led_blue_flash(void)
{
	led_set(&led_blue, 1);
	(void)k_work_reschedule(&led_blue_off_work, K_MSEC(80));
}

static int leds_init(void)
{
	if (!gpio_is_ready_dt(&led_blue) || !gpio_is_ready_dt(&led_green)) {
		LOG_ERR("LED gpio not ready");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
	led_blue_off();
	return 0;
}

static void antenna_switch_power_on(void)
{
	const struct device *gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
	int p105;

	if (!device_is_ready(gpio1)) {
		LOG_ERR("gpio1 not ready, P1.05 not driven");
		return;
	}

	/* 不要驱动 P1.07，避免和 SX1262 DIO2 抢开关。 */
	gpio_pin_configure(gpio1, ANT_SWITCH_CTRL_PIN, GPIO_INPUT);

	/*
	 * P1.05 是 RF 天线开关的供电（SX126x-Arduino USE_RXEN_ANT_PWR）。
	 * Bernd lora_rak4630_init：TX/RX 都 digitalWrite(37, HIGH)。
	 * 高电平才打开通路；板上 dts 的 GPIO_ACTIVE_LOW 是错的，别照抄。
	 */
	gpio_pin_configure(gpio1, ANT_SWITCH_PWR_PIN, GPIO_OUTPUT_HIGH);

	p105 = gpio_pin_get_raw(gpio1, ANT_SWITCH_PWR_PIN);
	LOG_INF("P1.05 raw=%d (expect 1) REGOUT0=0x%08x (3V3=5)", p105,
		(uint32_t)(NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) >> UICR_REGOUT0_VOUT_Pos);
}

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

	/* 天线开关供电是常开的（见 antenna_switch_power_on），这里不再重配
	 * 引脚：每次 uplink 前 gpio_pin_configure 会让脚短暂脱离输出态，
	 * 反而可能在 TX 瞬间把开关断电。 */
	smtc_modem_return_code_t rc =
		smtc_modem_request_uplink(STACK_ID, UPLINK_PORT, true, buff, sizeof(buff));
	if (rc != SMTC_MODEM_RC_OK) {
		LOG_ERR("uplink failed: %d", rc);
		return;
	}
	led_blue_flash();
	LOG_INF("confirmed uplink #%u on port %u", uplink_counter, UPLINK_PORT);
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
			leds_joining();
			check_rc("set_deveui", smtc_modem_set_deveui(STACK_ID, user_dev_eui));
			check_rc("set_joineui", smtc_modem_set_joineui(STACK_ID, user_join_eui));
			check_rc("set_appkey", smtc_modem_set_appkey(STACK_ID, user_gen_app_key));
			check_rc("set_nwkkey", smtc_modem_set_nwkkey(STACK_ID, user_app_key));
			check_rc("set_region", smtc_modem_set_region(STACK_ID, MODEM_REGION));
			/* 关掉 LoRaWAN Join 占空比退避（首小时约 toa/10，SF10 大约几十秒一次）。
			 * 调试用；量产应关掉，按规范退避。 */
			check_rc("join_dc_bypass",
				 smtc_modem_set_join_duty_cycle_backoff_bypass(STACK_ID, true));
			antenna_switch_power_on();
			check_rc("join", smtc_modem_join_network(STACK_ID));
			break;

		case SMTC_MODEM_EVENT_JOINED:
			LOG_INF("JOINED");
			leds_joined();
			send_counter();
			check_rc("alarm", smtc_modem_alarm_start_timer(PERIOD_S));
			break;

		case SMTC_MODEM_EVENT_JOINFAIL:
			LOG_WRN("JOINFAIL, retry in %u s", PERIOD_S);
			leds_joining();
			/* 取消 LBM 自己的 join 退避，改由 alarm 每 5 s 再 join */
			(void)smtc_modem_leave_network(STACK_ID);
			check_rc("alarm", smtc_modem_alarm_start_timer(PERIOD_S));
			break;

		case SMTC_MODEM_EVENT_ALARM: {
			smtc_modem_status_mask_t status = 0;

			check_rc("get_status", smtc_modem_get_status(STACK_ID, &status));
			if ((status & SMTC_MODEM_STATUS_JOINED) != 0) {
				send_counter();
				check_rc("alarm", smtc_modem_alarm_start_timer(PERIOD_S));
			} else {
				LOG_INF("join retry");
				check_rc("join", smtc_modem_join_network(STACK_ID));
			}
			break;
		}

		case SMTC_MODEM_EVENT_TXDONE: {
			smtc_modem_event_txdone_status_t st = event.event_data.txdone.status;

			if (st == SMTC_MODEM_EVENT_TXDONE_CONFIRMED) {
				LOG_INF("TXDONE ACK");
			} else if (st == SMTC_MODEM_EVENT_TXDONE_SENT) {
				LOG_WRN("TXDONE sent, no ACK");
			} else {
				LOG_WRN("TXDONE not sent (%d)", st);
			}
			led_blue_off();
			break;
		}

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
	(void)leds_init();
	leds_joining();

	wait_usb_console();

	LOG_INF("rzi USP LoRaWAN on %s", CONFIG_BOARD_TARGET);
	LOG_INF("DevEUI %02X%02X%02X%02X%02X%02X%02X%02X", user_dev_eui[0], user_dev_eui[1],
		user_dev_eui[2], user_dev_eui[3], user_dev_eui[4], user_dev_eui[5], user_dev_eui[6],
		user_dev_eui[7]);

	antenna_switch_power_on();

	SMTC_SW_PLATFORM_INIT();
	SMTC_SW_PLATFORM_VOID(smtc_rac_init());
	SMTC_SW_PLATFORM_VOID(smtc_modem_init(&modem_event_callback));

	LOG_INF("joining (edit keys in boards/rak4631_nrf52840.overlay)");

	k_sleep(K_FOREVER);
	return 0;
}
