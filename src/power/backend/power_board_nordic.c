/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief nRF52 and nRF54L low-power entry.
 *
 * Pin maps come from devicetree, so 4631 and 3362 share this file.
 * System OFF wakes only on a level. Pins that already have level sense
 * are left alone; every other pin returns to the reset state.
 */

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/iterable_sections.h>

#include <zephyr/dt-bindings/pinctrl/nrf-pinctrl.h>

#include <hal/nrf_clock.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_spim.h>

#if defined(CONFIG_SOC_SERIES_NRF52)
#include <hal/nrf_power.h>
#endif

#ifdef CONFIG_PM_DEVICE
#include <zephyr/pm/device.h>
#endif

#ifdef CONFIG_POWEROFF
#include <zephyr/sys/poweroff.h>
#endif

#if defined(CONFIG_SPI) && DT_HAS_CHOSEN(zephyr_lorawan_transceiver)
#include <zephyr/drivers/spi.h>
#endif

#include <rzi/err.h>

#include "power_board.h"

#if defined(CONFIG_SPI) && DT_HAS_CHOSEN(zephyr_lorawan_transceiver) && \
	(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_lorawan_transceiver), semtech_sx1261_new) || \
	 DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_lorawan_transceiver), semtech_sx1262_new) || \
	 DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_lorawan_transceiver), semtech_sx1268_new))
#define POWER_HAS_SX126X 1
#define LORA_NODE	 DT_CHOSEN(zephyr_lorawan_transceiver)
#define LORA_BUS	 DT_BUS(LORA_NODE)
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(lfclk)) && \
	DT_NODE_HAS_PROP(DT_NODELABEL(lfclk), k32src) && \
	DT_ENUM_HAS_VALUE(DT_NODELABEL(lfclk), k32src, xtal)
#define POWER_LFCLK_XTAL 1
#endif

#if defined(CONFIG_SOC_SERIES_NRF52) && defined(POWER_LFCLK_XTAL)
/* XL1/XL2 are P0.00 and P0.01. The input buffer sits in its linear region
 * on the crystal waveform unless the pad is disconnected.
 */
static void disconnect_lfxo_pins(void)
{
	nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(0, 0));
	nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(0, 1));
}
#endif

#ifdef POWER_HAS_SX126X
static const struct spi_dt_spec lora_spi = SPI_DT_SPEC_GET(
	LORA_NODE, SPI_WORD_SET(8) | SPI_OP_MODE_CONTROLLER | SPI_TRANSFER_MSB);

static void sx126x_write(uint8_t *cmd, uint8_t len)
{
	struct spi_buf tx = {.buf = cmd, .len = len};
	const struct spi_buf_set txs = {.buffers = &tx, .count = 1};

	(void)spi_write_dt(&lora_spi, &txs);
}

/* Warm sleep keeps the DIO3 TCXO switch off. Cold sleep clears it and the
 * TCXO supply floats.
 */
static void sx126x_sleep(void)
{
	uint8_t standby[] = {0x80, 0x00};
	uint8_t sleep_cmd[] = {0x84, 0x00};

	if (!spi_is_ready_dt(&lora_spi)) {
		return;
	}

#if DT_NODE_HAS_PROP(LORA_NODE, busy_gpios)
	const struct gpio_dt_spec busy = GPIO_DT_SPEC_GET(LORA_NODE, busy_gpios);

	if (gpio_is_ready_dt(&busy)) {
		gpio_pin_configure_dt(&busy, GPIO_INPUT);
		for (int i = 0; i < 50; i++) {
			if (gpio_pin_get_dt(&busy) == 0) {
				break;
			}
			k_usleep(100);
		}
	}
#endif

	sx126x_write(standby, sizeof(standby));

#if DT_NODE_HAS_PROP(LORA_NODE, dio3_as_tcxo_control)
	uint32_t ticks = DT_PROP(LORA_NODE, tcxo_wakeup_time) * 64U;
	uint8_t tcxo[] = {
		0x97,
		DT_PROP(LORA_NODE, tcxo_voltage),
		(uint8_t)(ticks >> 16),
		(uint8_t)(ticks >> 8),
		(uint8_t)ticks,
	};

	sx126x_write(tcxo, sizeof(tcxo));
	sleep_cmd[1] = 0x04;
#endif

	sx126x_write(sleep_cmd, sizeof(sleep_cmd));
	k_msleep(2);
}
#endif /* POWER_HAS_SX126X */

#ifdef CONFIG_PM_DEVICE
static void suspend_one(const struct device *dev)
{
	int rc = pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);

	ARG_UNUSED(rc);
}

static void suspend_devices(void)
{
	const struct device *console = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_console));
#if defined(POWER_HAS_SX126X)
	const struct device *spi = DEVICE_DT_GET(LORA_BUS);
#else
	const struct device *spi = NULL;
#endif

	if (spi != NULL) {
		suspend_one(spi);
	}

	STRUCT_SECTION_FOREACH(device, dev) {
		if (dev == spi || dev == console || DEVICE_API_IS(gpio, dev)) {
			continue;
		}
		suspend_one(dev);
	}
	if (console != NULL) {
		suspend_one(console);
	}
}
#else
static void suspend_devices(void)
{
}
#endif

static void spim_release(NRF_SPIM_Type *spim)
{
	nrf_spim_disable(spim);
	nrf_spim_pins_set(spim, NRF_SPIM_PIN_NOT_CONNECTED, NRF_SPIM_PIN_NOT_CONNECTED,
			  NRF_SPIM_PIN_NOT_CONNECTED);
#if defined(NRF_SPIM3)
	/* Same store as nrfx_spim_uninit() for errata 195. SPIM3 is not
	 * always passed to nrfx_spim_init(), so uninit() cannot be called.
	 */
	if (spim == NRF_SPIM3) {
		*(volatile uint32_t *)0x4002F004 = 1U;
	}
#endif
}

#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_spim)
#define RELEASE_SPIM(node) spim_release((NRF_SPIM_Type *)DT_REG_ADDR(node));
#endif

static void spims_release(void)
{
#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_spim)
	DT_FOREACH_STATUS_OKAY(nordic_nrf_spim, RELEASE_SPIM)
#endif
#if defined(NRF_SPIM3)
	spim_release(NRF_SPIM3);
#endif
}

#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_uarte)
#include <hal/nrf_uarte.h>

static void uarte_release(NRF_UARTE_Type *uarte)
{
	if (nrf_uarte_enable_check(uarte)) {
		nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_TXSTOPPED);
		nrf_uarte_task_trigger(uarte, NRF_UARTE_TASK_STOPTX);
		nrf_uarte_task_trigger(uarte, NRF_UARTE_TASK_STOPRX);
		for (int i = 0; i < 10000; i++) {
			if (nrf_uarte_event_check(uarte, NRF_UARTE_EVENT_TXSTOPPED)) {
				break;
			}
		}
	}
	nrf_uarte_disable(uarte);
	nrf_uarte_txrx_pins_disconnect(uarte);
	nrf_uarte_hwfc_pins_disconnect(uarte);
}

#define RELEASE_UARTE(node) uarte_release((NRF_UARTE_Type *)DT_REG_ADDR(node));

static void uartes_release(void)
{
	DT_FOREACH_STATUS_OKAY(nordic_nrf_uarte, RELEASE_UARTE)
}
#else
static void uartes_release(void)
{
}
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_uart)
#include <hal/nrf_uart.h>

static void uart_release(NRF_UART_Type *uart)
{
	nrf_uart_disable(uart);
	nrf_uart_txrx_pins_disconnect(uart);
	nrf_uart_hwfc_pins_disconnect(uart);
}

#define RELEASE_UART(node) uart_release((NRF_UART_Type *)DT_REG_ADDR(node));

static void uarts_release(void)
{
	DT_FOREACH_STATUS_OKAY(nordic_nrf_uart, RELEASE_UART)
}
#else
static void uarts_release(void)
{
}
#endif

#if defined(NRF_QSPI)
#include <hal/nrf_qspi.h>

static void qspi_release(void)
{
	nrf_qspi_pins_t pins = {
		.sck_pin = NRF_QSPI_PIN_NOT_CONNECTED,
		.csn_pin = NRF_QSPI_PIN_NOT_CONNECTED,
		.io0_pin = NRF_QSPI_PIN_NOT_CONNECTED,
		.io1_pin = NRF_QSPI_PIN_NOT_CONNECTED,
		.io2_pin = NRF_QSPI_PIN_NOT_CONNECTED,
		.io3_pin = NRF_QSPI_PIN_NOT_CONNECTED,
	};

	nrf_qspi_disable(NRF_QSPI);
	nrf_qspi_pins_set(NRF_QSPI, &pins);
}
#else
static void qspi_release(void)
{
}
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_timer)
#include <hal/nrf_timer.h>

static void timer_stop(NRF_TIMER_Type *timer)
{
	nrf_timer_task_trigger(timer, NRF_TIMER_TASK_STOP);
#if NRF_TIMER_HAS_SHUTDOWN
	nrf_timer_task_trigger(timer, NRF_TIMER_TASK_SHUTDOWN);
#endif
	nrf_timer_int_disable(timer, UINT32_MAX);
}

#define STOP_TIMER(node) timer_stop((NRF_TIMER_Type *)DT_REG_ADDR(node));

static void timers_stop(void)
{
	DT_FOREACH_STATUS_OKAY(nordic_nrf_timer, STOP_TIMER)
}
#else
static void timers_stop(void)
{
}
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_rtc)
#include <hal/nrf_rtc.h>

static void rtc_stop(NRF_RTC_Type *rtc)
{
	uint32_t sample;

	nrf_rtc_int_disable(rtc, UINT32_MAX);
	rtc->EVTENCLR = UINT32_MAX;
	nrf_rtc_task_trigger(rtc, NRF_RTC_TASK_STOP);
	nrf_rtc_event_clear(rtc, NRF_RTC_EVENT_TICK);
	nrf_rtc_event_clear(rtc, NRF_RTC_EVENT_OVERFLOW);
	for (uint32_t ch = 0; ch < NRF_RTC_CC_COUNT_MAX; ch++) {
		nrf_rtc_event_clear(rtc, nrf_rtc_compare_event_get((uint8_t)ch));
	}

	/* One 32 kHz tick is about 30 us. A running counter changes in this
	 * window; TASKS_STOP is repeated until the counter holds still.
	 */
	for (int attempt = 0; attempt < 3; attempt++) {
		bool held = true;

		sample = rtc->COUNTER;
		for (int i = 0; i < 8000; i++) {
			if (rtc->COUNTER != sample) {
				held = false;
				break;
			}
		}
		if (held) {
			break;
		}
		nrf_rtc_task_trigger(rtc, NRF_RTC_TASK_STOP);
	}
}

#define STOP_RTC(node) rtc_stop((NRF_RTC_Type *)DT_REG_ADDR(node));

static void rtcs_stop(void)
{
	DT_FOREACH_STATUS_OKAY(nordic_nrf_rtc, STOP_RTC)
}
#else
static void rtcs_stop(void)
{
}
#endif

#if defined(CONFIG_SOC_SERIES_NRF52)
/* Errata 187: a floating VBUSDETECT latches a 48 MHz request that ignores
 * HFCLKSTOP. VBUS is a dedicated pin, so PIN_CNF cannot pull it down.
 */
static void usb_clock_request_off(void)
{
#ifdef NRF_USBD
	NRF_USBD->INTENCLR = UINT32_MAX;
	NRF_USBD->USBPULLUP = 0U;
	NRF_USBD->ENABLE = 0U;
#endif
	*(volatile uint32_t *)0x4006EC00 = 0x00009375U;
	*(volatile uint32_t *)0x4006ED14 = 0U;
	*(volatile uint32_t *)0x4006EC14 = 0U;
	*(volatile uint32_t *)0x4006EC00 = 0x00009375U;
	(void)nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
}
#else
static void usb_clock_request_off(void)
{
}
#endif

static void hfclk_users_release(void)
{
#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_clock_hfclk)
	const struct device *hfclk = DEVICE_DT_GET_ONE(nordic_nrf_clock_hfclk);

	if (device_is_ready(hfclk)) {
		(void)clock_control_off(hfclk, NULL);
	}
#endif
}

/* HFCLKSTOP has no stopped event. The source leaves the crystal once the
 * clock has actually stopped. LFCLK stays up until System OFF so the RTC
 * stop check can see a clock that is still ticking.
 */
static void hfclk_stop_and_wait(void)
{
	usb_clock_request_off();
	nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTOP);
	for (int i = 0; i < 100000; i++) {
		if (nrf_clock_hf_src_get(NRF_CLOCK) != NRF_CLOCK_HFCLK_HIGH_ACCURACY) {
			break;
		}
	}
}

#if defined(CONFIG_SOC_SERIES_NRF52)
static void drop_ram_retention(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(NRF_POWER->RAM); i++) {
		NRF_POWER->RAM[i].POWERCLR = POWER_RAM_POWER_S0RETENTION_Msk |
					     POWER_RAM_POWER_S1RETENTION_Msk;
	}
}
#else
static void drop_ram_retention(void)
{
}
#endif

static bool gpio_level_sense(uint32_t abs_pin)
{
	uint32_t pin = abs_pin;
	NRF_GPIO_Type *reg = nrf_gpio_pin_port_decode(&pin);
	uint32_t sense = (reg->PIN_CNF[pin] & GPIO_PIN_CNF_SENSE_Msk) >> GPIO_PIN_CNF_SENSE_Pos;

	return sense == GPIO_PIN_CNF_SENSE_High || sense == GPIO_PIN_CNF_SENSE_Low;
}

static void drive_pin(uint32_t abs_pin, bool high)
{
	if (high) {
		nrf_gpio_pin_set(abs_pin);
	} else {
		nrf_gpio_pin_clear(abs_pin);
	}
	nrf_gpio_cfg(abs_pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
		     NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
}

static void default_gpio_port(uint32_t port, uint32_t count)
{
	for (uint32_t pin = 0; pin < count; pin++) {
		uint32_t abs_pin = NRF_GPIO_PIN_MAP(port, pin);

		if (!gpio_level_sense(abs_pin)) {
			nrf_gpio_cfg_default(abs_pin);
		}
	}
}

#define DEFAULT_GPIO(node) default_gpio_port(DT_PROP(node, port), DT_PROP(node, ngpios));

#ifdef POWER_HAS_SX126X
static void hold_spi_output(uint32_t psel)
{
	uint32_t function = (psel >> NRF_FUN_POS) & NRF_FUN_MSK;
	uint32_t abs_pin = psel & NRF_PIN_MSK;

	if (abs_pin == NRF_PIN_DISCONNECTED) {
		return;
	}
	if (function != NRF_FUN_SPIM_SCK && function != NRF_FUN_SPIM_MOSI) {
		return;
	}
	drive_pin(abs_pin, false);
}

#define HOLD_PSEL(node, prop, idx) hold_spi_output(DT_PROP_BY_IDX(node, prop, idx));
#define HOLD_GROUP(node)	   DT_FOREACH_PROP_ELEM(node, psels, HOLD_PSEL)
#endif

static void hold_board_pins(void)
{
#ifdef POWER_HAS_SX126X
#if DT_NODE_HAS_PROP(LORA_BUS, pinctrl_0)
	DT_FOREACH_CHILD(DT_PHANDLE_BY_IDX(LORA_BUS, pinctrl_0, 0), HOLD_GROUP)
#endif
#if DT_SPI_DEV_HAS_CS_GPIOS(LORA_NODE)
	drive_pin(NRF_GPIO_PIN_MAP(DT_PROP(DT_SPI_DEV_CS_GPIOS_CTLR(LORA_NODE), port),
				   DT_SPI_DEV_CS_GPIOS_PIN(LORA_NODE)),
		  (DT_SPI_DEV_CS_GPIOS_FLAGS(LORA_NODE) & GPIO_ACTIVE_LOW) != 0U);
#endif
#if DT_NODE_HAS_PROP(LORA_NODE, reset_gpios)
	drive_pin(NRF_GPIO_PIN_MAP(DT_PROP(DT_GPIO_CTLR(LORA_NODE, reset_gpios), port),
				   DT_GPIO_PIN(LORA_NODE, reset_gpios)),
		  (DT_GPIO_FLAGS(LORA_NODE, reset_gpios) & GPIO_ACTIVE_LOW) != 0U);
#endif
#endif /* POWER_HAS_SX126X */

#if DT_NODE_EXISTS(DT_NODELABEL(rf_power))
	drive_pin(NRF_GPIO_PIN_MAP(DT_PROP(DT_PARENT(DT_NODELABEL(rf_power)), port),
				   DT_GPIO_HOG_PIN_BY_IDX(DT_NODELABEL(rf_power), 0)),
		  true);
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(rf_control))
	drive_pin(NRF_GPIO_PIN_MAP(DT_PROP(DT_PARENT(DT_NODELABEL(rf_control)), port),
				   DT_GPIO_HOG_PIN_BY_IDX(DT_NODELABEL(rf_control), 0)),
		  false);
#endif
}

static void gpios_quiesce(void)
{
	DT_FOREACH_STATUS_OKAY(nordic_nrf_gpio, DEFAULT_GPIO)
	hold_board_pins();
#if defined(CONFIG_SOC_SERIES_NRF52) && defined(POWER_LFCLK_XTAL)
	disconnect_lfxo_pins();
#endif
#if defined(NRF_GPIO_LATCH_PRESENT)
	uint32_t masks[GPIO_COUNT];

	nrf_gpio_latches_read_and_clear(0, GPIO_COUNT, masks);
#endif
}

int power_board_prepare_retention(void)
{
#if defined(CONFIG_SOC_SERIES_NRF52) && defined(POWER_LFCLK_XTAL)
	disconnect_lfxo_pins();
#endif
	return 0;
}

int power_board_enter_off(void)
{
#ifdef POWER_HAS_SX126X
	sx126x_sleep();
#endif
	suspend_devices();
	hfclk_users_release();
	(void)irq_lock();
	spims_release();
	uartes_release();
	uarts_release();
	qspi_release();
	timers_stop();
	rtcs_stop();
	hfclk_stop_and_wait();
	drop_ram_retention();
	gpios_quiesce();
	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

#ifdef CONFIG_POWEROFF
	sys_poweroff();
#endif
	return -RZI_ERR_NOT_SUPPORTED;
}
