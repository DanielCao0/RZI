/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <rzi/err.h>
#include <rzi/init.h>
#include <rzi/lorawan/lorawan.h>
#include <rzi/power/power.h>

#if defined(CONFIG_SOC_SERIES_NRF52)
#include <hal/nrf_clock.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_power.h>
#include <hal/nrf_rng.h>
#include <hal/nrf_spim.h>
#include <nrfx_clock_hfclk.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/init.h>
#endif
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/usb/usbd.h>
#endif

static void ignore_result(int result)
{
	ARG_UNUSED(result);
}

/* Print only while a host has the CDC port open. poll_out blocks once the
 * USB buffer fills, which would pin the modem thread and the current.
 */
bool rzi_console_listening(void)
{
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
	const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0U;

	if (!device_is_ready(uart)) {
		return false;
	}
	if (uart_line_ctrl_get(uart, UART_LINE_CTRL_DTR, &dtr) != 0 || dtr == 0U) {
		return false;
	}
	return true;
#else
	return true;
#endif
}

LOG_MODULE_REGISTER(rzi_low_power, LOG_LEVEL_INF);

#define USER_NODE          DT_PATH(zephyr_user)
#define LORA_NODE          DT_CHOSEN(zephyr_lorawan_transceiver)
#define LORAWAN_REGION     RZI_LORAWAN_REGION_EU_868
#define LORAWAN_CLASS      RZI_LORAWAN_CLASS_A
/* Pin Join to DR5 so a slow Join does not burn the EU868 1% duty cycle.
 * After join, let the network run ADR. A custom/fixed DR NACKs LinkADRReq
 * (DR and power), LBM then sends a MAC-only uplink in 1-3 s, and the
 * network retries forever — about one extra uplink every 2 s.
 */
#define LORAWAN_ADR        true
#define LORAWAN_DATA_RATE  RZI_LORAWAN_DR_5
#define RETRY_DELAY_MS     5000
#define UPLINK_INTERVAL_MS 10000
/* The event queue drops TX_DONE on overflow, so never wait for it forever. */
#define TX_DONE_TIMEOUT_MS 120000
#define UPLINK_PORT        101

static K_SEM_DEFINE(stack_ready, 0, 1);
static K_SEM_DEFINE(join_done, 0, 1);
static K_SEM_DEFINE(tx_done, 0, 1);
static K_MUTEX_DEFINE(console_lock);
static volatile bool joined;

/* Callbacks run on the dispatcher thread while main may have the console
 * off. They only record what happened; main logs it with the console held.
 */
static struct k_spinlock event_lock;
static struct rzi_lorawan_tx_result last_tx;
static struct rzi_lorawan_downlink last_downlink;
static uint32_t downlink_count;
static int last_error;
static const struct device *console_uart;
#if !defined(CONFIG_PM) && DT_NODE_HAS_STATUS(DT_BUS(LORA_NODE), okay)
static const struct device *lora_spi_bus = DEVICE_DT_GET(DT_BUS(LORA_NODE));
#else
static const struct device *lora_spi_bus;
#endif

#if defined(CONFIG_SOC_SERIES_NRF52)
/* XL1/XL2 are P0.00/P0.01. The crystal waveform sits in the GPIO input
 * buffer's linear region, so a connected buffer draws about 0.5 mA and the
 * trace is a dense spike train. The analog oscillator does not use that buffer.
 */
static void nrf52_disconnect_lfxo_pins(void)
{
#if DT_ENUM_HAS_VALUE(DT_NODELABEL(lfclk), k32src, xtal) ||                                        \
	DT_ENUM_HAS_VALUE(DT_NODELABEL(lfclk), k32src, ext_low_swing) ||                           \
	DT_ENUM_HAS_VALUE(DT_NODELABEL(lfclk), k32src, ext_full_swing)
	nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(0, 0));
	nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(0, 1));
#endif
}

static int nrf52_lfxo_pins_init(void)
{
	nrf52_disconnect_lfxo_pins();
	return 0;
}

SYS_INIT(nrf52_lfxo_pins_init, PRE_KERNEL_1, 0);

#define NRF52_UNUSED(label) !DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(label))

/* MCUboot logs on uart1 and jumps here with it still enabled. The app's
 * devicetree disables it, so no driver ever turns it off, and its clock
 * request keeps System ON idle near 1 mA. Power-cycle every block that no
 * driver owns: offset 0xFFC is the peripheral power switch used by the
 * errata workarounds, and 0 then 1 resets the block and drops its request.
 */
static void nrf52_release_unused_peripherals(void)
{
	static const uintptr_t unused[] = {
#if NRF52_UNUSED(uart0)
		NRF_UARTE0_BASE,
#endif
#if NRF52_UNUSED(spi0) && NRF52_UNUSED(i2c0)
		NRF_SPIM0_BASE,
#endif
#if NRF52_UNUSED(spi1) && NRF52_UNUSED(i2c1)
		NRF_SPIM1_BASE,
#endif
#if NRF52_UNUSED(nfct)
		NRF_NFCT_BASE,
#endif
#if NRF52_UNUSED(adc)
		NRF_SAADC_BASE,
#endif
#if NRF52_UNUSED(timer0)
		NRF_TIMER0_BASE,
#endif
#if NRF52_UNUSED(timer1)
		NRF_TIMER1_BASE,
#endif
#if NRF52_UNUSED(timer2)
		NRF_TIMER2_BASE,
#endif
#if NRF52_UNUSED(comp)
		NRF_COMP_BASE,
#endif
#if NRF52_UNUSED(qdec)
		NRF_QDEC_BASE,
#endif
#if NRF52_UNUSED(timer3)
		NRF_TIMER3_BASE,
#endif
#if NRF52_UNUSED(timer4)
		NRF_TIMER4_BASE,
#endif
#if NRF52_UNUSED(pwm0)
		NRF_PWM0_BASE,
#endif
#if NRF52_UNUSED(pdm0)
		NRF_PDM_BASE,
#endif
#if NRF52_UNUSED(pwm1)
		NRF_PWM1_BASE,
#endif
#if NRF52_UNUSED(pwm2)
		NRF_PWM2_BASE,
#endif
#if NRF52_UNUSED(spi2)
		NRF_SPIM2_BASE,
#endif
#if NRF52_UNUSED(i2s0)
		NRF_I2S_BASE,
#endif
#if NRF52_UNUSED(uart1)
		NRF_UARTE1_BASE,
#endif
#if NRF52_UNUSED(qspi)
		NRF_QSPI_BASE,
#endif
#if NRF52_UNUSED(pwm3)
		NRF_PWM3_BASE,
#endif
#if NRF52_UNUSED(spi3)
		NRF_SPIM3_BASE,
#endif
	};

	for (size_t i = 0; i < ARRAY_SIZE(unused); i++) {
		volatile uint32_t *power = (volatile uint32_t *)(unused[i] + 0xFFCU);

		*power = 0U;
		(void)*power;
		*power = 1U;
	}
}

/* USB 48 MHz leak only. The 32 MHz crystal is stopped later, in the idle gap,
 * after the SX1262 has been put to sleep. Stopping it while a SPI command is
 * in progress leaves the radio in STDBY_RC at about 0.8 mA.
 */
static void nrf52_quiesce(void)
{
	nrf52_disconnect_lfxo_pins();

	/* Errata 187/211: 3 keeps the USB 48 MHz clock request alive after
	 * USBD is disabled. The register ignores writes unless 0x4006EC00 is
	 * unlocked first.
	 */
	/* Always unlock. After the first clear the key register is no longer 0,
	 * and a conditional write leaves the USB 48 MHz request latched. That
	 * request holds the 32 MHz crystal at about 0.8 mA with the radio asleep.
	 */
	*(volatile uint32_t *)0x4006EC00 = 0x00009375U;
	*(volatile uint32_t *)0x4006ED14 = 0U;
	*(volatile uint32_t *)0x4006EC00 = 0x00009375U;
	nrf_power_int_disable(NRF_POWER, NRF_POWER_INT_USBDETECTED_MASK |
					     NRF_POWER_INT_USBREMOVED_MASK |
					     NRF_POWER_INT_USBPWRRDY_MASK);
	nrf_power_event_clear(NRF_POWER, NRF_POWER_EVENT_USBDETECTED);
	nrf_power_event_clear(NRF_POWER, NRF_POWER_EVENT_USBREMOVED);
	nrf_power_event_clear(NRF_POWER, NRF_POWER_EVENT_USBPWRRDY);
	nrf_power_task_trigger(NRF_POWER, NRF_POWER_TASK_LOWPWR);

#ifdef NRF_USBD
	/* Mask the IRQ before ENABLE is cleared, or a stuck event wakes the CPU. */
	irq_disable(USBD_IRQn);
	NVIC_ClearPendingIRQ(USBD_IRQn);
	NRF_USBD->INTENCLR = 0xFFFFFFFFU;
	NRF_USBD->USBPULLUP = 0U;
	NRF_USBD->ENABLE = 0U;
	{
		volatile uint32_t *power = (volatile uint32_t *)((uintptr_t)NRF_USBD + 0xFFCU);

		*power = 0U;
		(void)*power;
		*power = 1U;
	}
	*(volatile uint32_t *)0x4006EC00 = 0x00009375U;
	*(volatile uint32_t *)0x4006ED14 = 0U;
	*(volatile uint32_t *)0x4006EC00 = 0x00009375U;
#endif
}

#if defined(CONFIG_USP_THREADS_MUTEXES)
extern struct k_mutex rac_api_mutex;
#endif

extern int sx126x_hal_write(const void *context, const uint8_t *command,
			    const uint16_t command_length, const uint8_t *data,
			    const uint16_t data_length);
extern void lora_transceiver_board_disable_interrupt(const struct device *dev);
extern void lora_transceiver_board_enable_interrupt(const struct device *dev);
extern bool rp_radio_is_free(void *rp);
extern void *smtc_rac_get_rp(void);

#define SPI1_SLEEP_GRP1 DT_CHILD(DT_NODELABEL(spi1_sleep), group1)
#define SPI1_SLEEP_GRP2 DT_CHILD(DT_NODELABEL(spi1_sleep), group2)
/* Low 9 bits of an nRF psel are the absolute GPIO number. */
#define SPI1_PSEL_PIN(node, idx) (DT_PROP_BY_IDX(node, psels, idx) & 0x1FFU)

/* Sleep pinctrl writes SCK and MOSI as GPIO outputs, but it also leaves them
 * selected by SPIM. With the block disabled those pins are high-Z, and an
 * SX1262 that is still in standby oscillates between about 0.2 mA and 4 mA.
 * Disconnect the peripheral, then drive the outputs from GPIO.
 */
static void nrf52_park_radio_pins(NRF_SPIM_Type *spim)
{
	uint32_t sck = SPI1_PSEL_PIN(SPI1_SLEEP_GRP1, 0);
	uint32_t mosi = SPI1_PSEL_PIN(SPI1_SLEEP_GRP1, 1);
	uint32_t miso = SPI1_PSEL_PIN(SPI1_SLEEP_GRP2, 0);
	uint32_t nss = NRF_GPIO_PIN_MAP(DT_PROP(DT_GPIO_CTLR(DT_BUS(LORA_NODE), cs_gpios), port),
					DT_GPIO_PIN(DT_BUS(LORA_NODE), cs_gpios));

	nrf_gpio_pin_clear(sck);
	nrf_gpio_cfg(sck, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
		     NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_pin_clear(mosi);
	nrf_gpio_cfg(mosi, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
		     NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_cfg(miso, NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
		     NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
	nrf_spim_pins_set(spim, NRF_SPIM_PIN_NOT_CONNECTED, NRF_SPIM_PIN_NOT_CONNECTED,
			  NRF_SPIM_PIN_NOT_CONNECTED);
	nrf_gpio_pin_set(nss);
	nrf_gpio_cfg_output(nss);
}

/* DIO1 is armed with edge sense. DETECT wakes WFE even though PRIMASK is
 * set, and the CPU then runs from flash at about 4 mA until the pin is
 * quiet. With a wake every 1.5 ms the average lands near 750 µA while the
 * floor between peaks stays in the tens of µA.
 */
static void nrf52_block_gpio_wake(void)
{
	for (int pin = 0; pin < 32; pin++) {
		NRF_P0->PIN_CNF[pin] &= ~GPIO_PIN_CNF_SENSE_Msk;
		NRF_P1->PIN_CNF[pin] &= ~GPIO_PIN_CNF_SENSE_Msk;
	}
	NRF_P0->LATCH = 0xFFFFFFFFU;
	NRF_P1->LATCH = 0xFFFFFFFFU;
	NRF_GPIOTE->INTENCLR = 0xFFFFFFFFU;
	for (int ch = 0; ch < 8; ch++) {
		NRF_GPIOTE->CONFIG[ch] = 0U;
	}
	/* BUSY and DIO1. Input buffers on these nets are not needed once
	 * the radio is asleep.
	 */
	nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(1, 14));
	nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(1, 15));
}

static void nrf52_clamp_rf_control(bool clamp);
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
static bool cdc_console_off(void);
#endif

/* System ON plus SX1262 warm sleep, for the power-up current measurement.
 * irq_lock() only raises BASEPRI. A pending PendSV still makes WFE return
 * immediately, and the CPU spins at about 3 mA while the 32 MHz crystal
 * is running. The working path masks with __disable_irq() and clears PendSV.
 *
 * SetSleep 0x84 0x04 only. Arming the DIO3 TCXO switch here leaves the
 * SX1262 in standby. With SCK and MOSI high-Z the module current then jumps
 * between about 0.2 mA and 4 mA, a few times a second, and the average sits
 * near 500 µA. A falling edge on NSS also wakes warm sleep into STDBY_RC.
 */
/* Errata 187 writes 3 to 0x4006ED14 while USB is enabled. That bit keeps
 * requesting the 48 MHz clock, and the 32 MHz crystal stays on with it.
 * Leave the power switch at its reset value. Writing it to 0 latches the
 * request, and writing it back to 1 while VBUSDETECT is set arms it again.
 */
static void nrf52_usb_clock_off(void)
{
#ifdef NRF_USBD
	NRF_USBD->INTENCLR = 0xFFFFFFFFU;
	NRF_USBD->USBPULLUP = 0U;
	NRF_USBD->ENABLE = 0U;
#endif
	*(volatile uint32_t *)0x4006EC00 = 0x00009375U;
	*(volatile uint32_t *)0x4006ED14 = 0U;
	*(volatile uint32_t *)0x4006EC14 = 0U;
	*(volatile uint32_t *)0x4006EC00 = 0x00009375U;
}

static void nrf52_system_on_warm_sleep(void)
{
	const struct device *radio = DEVICE_DT_GET(LORA_NODE);
	NRF_SPIM_Type *spim = (NRF_SPIM_Type *)DT_REG_ADDR(DT_BUS(LORA_NODE));
	static const uintptr_t periph[] = {
		NRF_UARTE0_BASE, NRF_SPIM0_BASE,  NRF_SPIM1_BASE,  NRF_SAADC_BASE,
		NRF_TIMER0_BASE, NRF_TIMER1_BASE, NRF_TIMER2_BASE, NRF_TEMP_BASE,	NRF_RNG_BASE,
		NRF_ECB_BASE,	 NRF_CCM_BASE,	  NRF_COMP_BASE,   NRF_QDEC_BASE,	NRF_TIMER3_BASE,
		NRF_TIMER4_BASE, NRF_PWM0_BASE,	  NRF_PDM_BASE,	   NRF_PWM1_BASE,	NRF_PWM2_BASE,
		NRF_SPIM2_BASE,	 NRF_I2S_BASE,	  NRF_UARTE1_BASE, NRF_QSPI_BASE,
		NRF_PWM3_BASE,	 NRF_SPIM3_BASE,
	};
	uint8_t warm_sleep[] = {0x84, 0x04};
	uint32_t nreset = NRF_GPIO_PIN_MAP(1, 6);

	/* Active for 10 s after power-up, then sleep for the meter. */
	k_msleep(10000);
	nrf52_disconnect_lfxo_pins();
	nrf_gpio_pin_set(nreset);
	nrf_gpio_cfg_output(nreset);
	if (device_is_ready(radio)) {
		lora_transceiver_board_disable_interrupt(radio);
		(void)sx126x_hal_write(radio, warm_sleep, sizeof(warm_sleep), NULL, 0);
	}
	nrf52_park_radio_pins(spim);
	nrf52_clamp_rf_control(true);
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
	/* Releases the clock-control request taken when USB was enabled. */
	(void)cdc_console_off();
	k_msleep(2);
#endif
	/* Drivers are still running. A peripheral IRQ during the shutdown
	 * turns SPI or USB back on and HFCLKSTOP is ignored, leaving the
	 * 32 MHz crystal at about 900 µA. Mask exceptions first.
	 */
	SCB->SCR &= ~SCB_SCR_SEVONPEND_Msk;
	SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
	SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;
	__disable_irq();
	__DSB();
	__ISB();

	NRF_UARTE0->TASKS_STOPRX = 1U;
	NRF_UARTE0->TASKS_STOPTX = 1U;
	NRF_UARTE0->ENABLE = 0U;
	NRF_UARTE1->TASKS_STOPRX = 1U;
	NRF_UARTE1->TASKS_STOPTX = 1U;
	NRF_UARTE1->ENABLE = 0U;
	NRF_SPIM0->ENABLE = 0U;
	NRF_SPIM1->ENABLE = 0U;
	NRF_SPIM2->ENABLE = 0U;
	NRF_SPIM3->ENABLE = 0U;
	NRF_TWIM0->ENABLE = 0U;
	NRF_TWIM1->ENABLE = 0U;
	NRF_SAADC->ENABLE = 0U;
	nrf52_usb_clock_off();
	NRF_RNG->TASKS_STOP = 1U;
	NRF_TIMER0->TASKS_STOP = 1U;
	NRF_TIMER0->TASKS_SHUTDOWN = 1U;
	NRF_TIMER1->TASKS_STOP = 1U;
	NRF_TIMER1->TASKS_SHUTDOWN = 1U;
	NRF_TIMER2->TASKS_STOP = 1U;
	NRF_TIMER2->TASKS_SHUTDOWN = 1U;
	NRF_TIMER3->TASKS_STOP = 1U;
	NRF_TIMER3->TASKS_SHUTDOWN = 1U;
	NRF_TIMER4->TASKS_STOP = 1U;
	NRF_TIMER4->TASKS_SHUTDOWN = 1U;
	SysTick->CTRL = 0U;
	for (int ch = 0; ch < 8; ch++) {
		NRF_GPIOTE->CONFIG[ch] = 0U;
	}
	NRF_PPI->CHEN = 0U;
	NRF_CRYPTOCELL->ENABLE = 0U;
	NRF_POWER->INTENCLR = 0xFFFFFFFFU;
	NRF_CLOCK->INTENCLR = 0xFFFFFFFFU;
	NRF_RADIO->TASKS_DISABLE = 1U;
	NRF_RADIO->POWER = 0U;
	/* ENABLE = 0 does not always drop a peripheral's HFCLK request.
	 * 0 then 1 resets the block and releases its clock. Leaving the
	 * switch at 0 keeps that request latched, so HFCLKSTOP is ignored
	 * and System ON stays near 850 µA.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(periph); i++) {
		volatile uint32_t *power = (volatile uint32_t *)(periph[i] + 0xFFCU);

		*power = 0U;
		(void)*power;
		*power = 1U;
	}
	/* Errata 195: SPIM3 keeps drawing current after disable. */
	*(volatile uint32_t *)0x4002F004 = 1U;
	/* NSS must stay high. A falling edge wakes warm sleep into STDBY_RC,
	 * about 0.6 mA, and with the crystal still running the sum is 1.4 mA.
	 * SCK is only re-driven here; it does not wake a sleeping SX1262.
	 */
	nrf52_park_radio_pins(spim);
	nrf52_clamp_rf_control(true);
	nrf52_block_gpio_wake();
	nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_CTSTOP);
	NRF_CLOCK->CTIV = 0U;
#if defined(CONFIG_ARMV7_M_ARMV8_M_FP)
	/* A pending FPU exception wakes WFE. Zephyr's idle clears it; this
	 * loop does not go through that path.
	 */
	__set_FPSCR(__get_FPSCR() & ~0x9FU);
	(void)__get_FPSCR();
	NVIC_ClearPendingIRQ(FPU_IRQn);
#endif
	NRF_RTC0->TASKS_STOP = 1U;
	NRF_RTC1->TASKS_STOP = 1U;
	NRF_RTC2->TASKS_STOP = 1U;
	nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_LFCLKSTOP);
	nrf_power_task_trigger(NRF_POWER, NRF_POWER_TASK_LOWPWR);
	for (volatile int w = 0; w < 2000; w++) {
	}
	nrf52_usb_clock_off();
	nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTOP);
	/* Park again. The SPIM power cycle above drops PSEL and the pads go
	 * high-Z until GPIO owns them. NSS stays high so warm sleep is not
	 * woken back into STDBY_RC.
	 */
	nrf52_park_radio_pins(spim);
	nrf52_clamp_rf_control(true);
	for (;;) {
		__WFE();
		__WFE();
	}
}

/* P1.07 is the rf_control hog: input buffer connected, no pull. On a bare
 * module that pad floats. While the SX1262 is asleep, DIO2 is Hi-Z too, so
 * hold this net low until the next radio task.
 */
static void nrf52_clamp_rf_control(bool clamp)
{
	uint32_t pin = NRF_GPIO_PIN_MAP(1, 7);

	if (clamp) {
		nrf_gpio_pin_clear(pin);
		nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
			     NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
	} else {
		nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_INPUT_CONNECT,
			     NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
	}
}

#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
static bool cdc_console_off(void);
#endif

/* Called by the modem thread with rac_api_mutex held, just before it sleeps.
 * That is the packet gap: RX windows included. The application idle runs
 * only after TX_DONE, so changes made only there never showed up.
 * 0x84 0x04 is warm sleep. It is sent only when the radio planner has no
 * running task, so an open RX window is left alone.
 */
void rzi_radio_gap_quiesce(void)
{
	const struct device *radio = DEVICE_DT_GET(LORA_NODE);
	const struct device *clk = DEVICE_DT_GET_ONE(nordic_nrf_clock_hfclk);
	NRF_SPIM_Type *spim = (NRF_SPIM_Type *)DT_REG_ADDR(DT_BUS(LORA_NODE));
	void *rp = smtc_rac_get_rp();
	bool radio_free = (rp == NULL) || rp_radio_is_free(rp);
	uint8_t warm_sleep[] = {0x84, 0x04};

	bool hf_before = nrf_clock_is_running(NRF_CLOCK, NRF_CLOCK_DOMAIN_HFCLK, NULL);
	bool spim_before = nrf_spim_enable_check(spim);
	bool vbus = nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
	int usage = lora_spi_bus != NULL ? pm_device_runtime_usage(lora_spi_bus) : -1;
	int sleep_rc = -1;

	if (radio_free && device_is_ready(radio)) {
		lora_transceiver_board_disable_interrupt(radio);
		sleep_rc = sx126x_hal_write(radio, warm_sleep, sizeof(warm_sleep), NULL, 0);
		nrf52_clamp_rf_control(true);
	}
	for (int i = 0; i < 20 && nrf_spim_enable_check(spim); i++) {
		k_msleep(1);
	}
	/* Sleep pinctrl leaves PSEL connected, so a GPIO write never reaches
	 * the pin and SCK/MOSI stay Hi-Z. Disconnect PSEL, then drive them.
	 */
	if (nrf_spim_enable_check(spim)) {
		nrf_spim_disable(spim);
	}
	nrf52_park_radio_pins(spim);
	nrf_rng_task_trigger(NRF_RNG, NRF_RNG_TASK_STOP);
	/* A running TIMER (errata 78), UART left on by MCUboot, or a disabled
	 * SPIM can still request the 32 MHz clock. HFCLKSTOP is ignored until
	 * that request drops, which is the flat ~800 µA. RTC stays up for k_sleep.
	 */
	NRF_TIMER0->TASKS_SHUTDOWN = 1U;
	NRF_TIMER1->TASKS_SHUTDOWN = 1U;
	NRF_TIMER2->TASKS_SHUTDOWN = 1U;
	NRF_TIMER3->TASKS_SHUTDOWN = 1U;
	NRF_TIMER4->TASKS_SHUTDOWN = 1U;
	NRF_PPI->CHENCLR = 0xFFFFFFFFU;
	{
		static const uintptr_t blocks[] = {
			NRF_UARTE0_BASE, NRF_UARTE1_BASE, NRF_SPIM0_BASE, NRF_SPIM2_BASE,
			NRF_SPIM3_BASE,	 NRF_TWIM0_BASE,  NRF_TWIM1_BASE,  NRF_SAADC_BASE,
			NRF_PWM0_BASE,	 NRF_PWM1_BASE,	  NRF_PWM2_BASE,  NRF_PWM3_BASE,
			NRF_QSPI_BASE,	 NRF_NFCT_BASE,
		};

		for (size_t i = 0; i < ARRAY_SIZE(blocks); i++) {
			volatile uint32_t *power = (volatile uint32_t *)(blocks[i] + 0xFFCU);

			*power = 0U;
			(void)*power;
			*power = 1U;
		}
		if (lora_spi_bus == NULL || pm_device_runtime_usage(lora_spi_bus) <= 0) {
			volatile uint32_t *power = (volatile uint32_t *)(NRF_SPIM1_BASE + 0xFFCU);

			*power = 0U;
			(void)*power;
			*power = 1U;
		}
	}
	/* DTR stays set after the cable is pulled, and VBUSDETECT stays set
	 * because the pin is only AC-coupled to ground. Either bit left the
	 * 32 MHz crystal running, so the gap current kept jumping. Always
	 * drop USB and the crystal here.
	 */
	k_mutex_lock(&console_lock, K_FOREVER);
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
	(void)cdc_console_off();
#endif
	nrf52_quiesce();
	if (device_is_ready(clk)) {
		(void)clock_control_off(clk, NULL);
	}
	irq_disable(RNG_IRQn);
	nrf_rng_task_trigger(NRF_RNG, NRF_RNG_TASK_STOP);
	/* HFCLKSTAT.STATE stays 1 on HFINT, so that bit cannot be used to
	 * decide. SRC bit 0 is the crystal.
	 */
	for (int n = 0; n < 3 && (NRF_CLOCK->HFCLKSTAT & CLOCK_HFCLKSTAT_SRC_Msk) != 0U; n++) {
		*(volatile uint32_t *)0x4006EC00 = 0x00009375U;
		*(volatile uint32_t *)0x4006ED14 = 0U;
		*(volatile uint32_t *)0x4006EC00 = 0x00009375U;
		nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTOP);
		k_busy_wait(200);
	}
	k_mutex_unlock(&console_lock);
	{
		static int32_t last_ms;
		static int last_key = -1;
		bool hf_after = nrf_clock_is_running(NRF_CLOCK, NRF_CLOCK_DOMAIN_HFCLK, NULL);
		bool spim_after = nrf_spim_enable_check(spim);
		int32_t now = k_uptime_get_32();
		int key = (radio_free ? 1 : 0) | (sleep_rc == 0 ? 2 : 0) | (spim_before ? 4 : 0) |
			  (spim_after ? 8 : 0) | (hf_before ? 16 : 0) | (hf_after ? 32 : 0) |
			  (vbus ? 64 : 0);

		if (rzi_console_listening() && (key != last_key || (now - last_ms) >= 1000)) {
			last_key = key;
			last_ms = now;
			printk("gap t=%d free=%d sleep=%d spim=%d->%d hf=%d->%d vbus=%d usage=%d\n",
			       now, radio_free, sleep_rc, spim_before, spim_after, hf_before, hf_after,
			       vbus, usage);
		}
	}
}

void rzi_radio_gap_resume(void)
{
	const struct device *radio = DEVICE_DT_GET(LORA_NODE);

	nrf52_clamp_rf_control(false);
	if (device_is_ready(radio)) {
		lora_transceiver_board_enable_interrupt(radio);
	}
}
#endif

#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
/* Boot-time CDC ACM context. Disabling it releases the 32 MHz crystal. */
static struct usbd_context *cdc_usbd(void)
{
	STRUCT_SECTION_FOREACH(usbd_context, ctx) {
		return ctx;
	}
	return NULL;
}

static void cdc_console_on(void)
{
	/* USB is enabled at boot only. Turning it back on here restarts the
	 * 32 MHz crystal: VBUSDETECT stays set with the cable unplugged.
	 */
}

/* Returns true once USB is off. DTR and VBUSDETECT both stay set on this
 * module after the cable is removed, so neither one may keep USB alive.
 */
static bool cdc_console_off(void)
{
	struct usbd_context *uds = cdc_usbd();

	if (uds == NULL) {
		return true;
	}
	(void)usbd_disable(uds);
	return true;
}
#endif

/* Drop the console only after its last line has left the UART. The driver
 * waits forever for TC if it is suspended again before any byte is sent.
 * CDC ACM has no device PM; turn the USB device off so the HF crystal stops.
 */
static void console_hold(void)
{
	k_mutex_lock(&console_lock, K_FOREVER);
	if (device_is_ready(console_uart)) {
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
		cdc_console_on();
#else
		(void)pm_device_action_run(console_uart, PM_DEVICE_ACTION_RESUME);
		pm_device_busy_set(console_uart);
#endif
	}
	k_mutex_unlock(&console_lock);
}

static void console_release(void)
{
	bool console_off = true;

	k_mutex_lock(&console_lock, K_FOREVER);
	if (device_is_ready(console_uart)) {
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
		console_off = cdc_console_off();
#else
		log_flush();
		pm_device_busy_clear(console_uart);
		(void)pm_device_action_run(console_uart, PM_DEVICE_ACTION_SUSPEND);
#endif
	}
#if defined(CONFIG_SOC_SERIES_NRF52)
	if (console_off) {
		nrf52_quiesce();
	}
#endif
	ARG_UNUSED(console_off);
	k_mutex_unlock(&console_lock);
}

/* Suspend the radio SPI bus on SoCs without system-managed PM (e.g. nRF52840)
 * when the driver does not use PM_DEVICE_RUNTIME.
 */
static void radio_bus_suspend(void)
{
	if (lora_spi_bus != NULL && device_is_ready(lora_spi_bus) &&
	    !pm_device_runtime_is_enabled(lora_spi_bus)) {
		(void)pm_device_action_run(lora_spi_bus, PM_DEVICE_ACTION_SUSPEND);
	}
}

static void radio_bus_resume(void)
{
	if (lora_spi_bus != NULL && device_is_ready(lora_spi_bus) &&
	    !pm_device_runtime_is_enabled(lora_spi_bus)) {
		(void)pm_device_action_run(lora_spi_bus, PM_DEVICE_ACTION_RESUME);
	}
}

static const char *tx_status_str(enum rzi_lorawan_tx_status status)
{
	switch (status) {
	case RZI_LORAWAN_TX_SENT:
		return "sent";
	case RZI_LORAWAN_TX_ACKED:
		return "acked";
	case RZI_LORAWAN_TX_NOT_SENT:
		return "not sent";
	default:
		return "unknown";
	}
}

static void on_event(const struct rzi_lorawan_event *event, void *user_data)
{
	k_spinlock_key_t key;

	ARG_UNUSED(user_data);
	switch (event->type) {
	case RZI_LORAWAN_EVENT_READY:
		k_sem_give(&stack_ready);
		break;
	case RZI_LORAWAN_EVENT_STATE_CHANGED:
		if (event->state == RZI_LORAWAN_STATE_READY) {
			k_sem_give(&stack_ready);
		}
		break;
	case RZI_LORAWAN_EVENT_JOINED:
		joined = true;
		k_sem_give(&join_done);
		break;
	case RZI_LORAWAN_EVENT_JOIN_FAILED:
		joined = false;
		k_sem_give(&join_done);
		break;
	case RZI_LORAWAN_EVENT_TX_DONE:
		key = k_spin_lock(&event_lock);
		last_tx = event->tx;
		k_spin_unlock(&event_lock, key);
		k_sem_give(&tx_done);
		break;
	case RZI_LORAWAN_EVENT_DOWNLINK:
		key = k_spin_lock(&event_lock);
		last_downlink = event->downlink;
		last_downlink.data = NULL;
		downlink_count++;
		k_spin_unlock(&event_lock, key);
		break;
	case RZI_LORAWAN_EVENT_ERROR:
		key = k_spin_lock(&event_lock);
		last_error = event->error;
		k_spin_unlock(&event_lock, key);
		break;
	default:
		break;
	}
}

/* Call with the console held. */
static void report_events(bool tx_finished)
{
	struct rzi_lorawan_tx_result tx;
	struct rzi_lorawan_downlink downlink;
	uint32_t downlinks;
	int error;
	k_spinlock_key_t key = k_spin_lock(&event_lock);

	tx = last_tx;
	downlink = last_downlink;
	downlinks = downlink_count;
	error = last_error;
	downlink_count = 0U;
	last_error = 0;
	k_spin_unlock(&event_lock, key);

	if (tx_finished) {
		if (tx.error != 0) {
			LOG_WRN("Uplink %s: %s", tx_status_str(tx.status), rzi_err_str(tx.error));
		} else {
			LOG_INF("Uplink %s", tx_status_str(tx.status));
		}
	}
	if (downlinks != 0U) {
		LOG_INF("%u downlink(s), last: port %u, %u bytes, RSSI %d dBm, SNR %d/4 dB",
			downlinks, downlink.port, (unsigned int)downlink.size, downlink.rssi_dbm,
			downlink.snr_quarter_db);
	}
	if (error != 0) {
		LOG_ERR("Error: %s", rzi_err_str(error));
	}
}

/* Class and ADR apply only after join; USP rejects set_class() before it.
 * Network ADR accepts LinkADRReq so the NS stops retrying.
 */
static void configure_after_join(void)
{
	int rc = rzi_lorawan_set_class(LORAWAN_CLASS);

	if (rc != 0) {
		LOG_ERR("Class failed: %s", rzi_err_str(rc));
	}
	if (LORAWAN_ADR) {
		rc = rzi_lorawan_set_adr(true);
	} else {
		rc = rzi_lorawan_set_data_rate(LORAWAN_DATA_RATE);
	}
	if (rc != 0) {
		LOG_ERR("%s failed: %s", LORAWAN_ADR ? "ADR" : "Data rate", rzi_err_str(rc));
	}
}

static const struct rzi_lorawan_callbacks callbacks = {
	.on_event = on_event,
};

/* Every slot holds the same data rate so Join uses that rate only. */
static int pin_join_data_rate(void)
{
	uint8_t distribution[RZI_LORAWAN_DR_DISTRIBUTION_LENGTH];

	memset(distribution, LORAWAN_DATA_RATE, sizeof(distribution));
	return rzi_lorawan_set_join_data_rates(distribution);
}

static int32_t duty_cycle_wait_ms(void)
{
	int32_t time_ms = 0;
	int rc = rzi_lorawan_get_duty_cycle_status(&time_ms);

	if (rc != 0 || time_ms >= 0) {
		return 0;
	}
	LOG_INF("Duty cycle wait %d ms", -time_ms);
	return -time_ms;
}

static int request_join(void)
{
	int rc;

	joined = false;
	k_sem_reset(&join_done);
	rc = rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 0);
	if (rc != 0) {
		LOG_WRN("Join rejected: %s", rzi_err_str(rc));
		return rc;
	}
	LOG_INF("Joining");
	console_release();
	k_sem_take(&join_done, K_FOREVER);
	console_hold();
	report_events(false);
	if (!joined) {
		LOG_WRN("Join failed, retrying in %d s", RETRY_DELAY_MS / 1000);
		return -RZI_ERR_NOT_READY;
	}
	return 0;
}

/* UART and radio SPI bus stay suspended for the whole idle gap. */
static void idle_with_console_off(int32_t timeout_ms)
{
#if defined(CONFIG_SOC_SERIES_NRF52) && defined(CONFIG_USP_THREADS_MUTEXES)
	bool busy = false;
	bool hold = rzi_lorawan_is_busy(&busy) != 0 || !busy;
#endif

	if (timeout_ms <= 0) {
		return;
	}
#if defined(CONFIG_SOC_SERIES_NRF52) && defined(CONFIG_USP_THREADS_MUTEXES)
	if (rzi_console_listening()) {
		printk("idle t=%d ms=%d hold=%d busy=%d\n", k_uptime_get_32(), timeout_ms, hold, busy);
	}
#endif
	console_release();
	radio_bus_suspend();
#if defined(CONFIG_SOC_SERIES_NRF52) && defined(CONFIG_USP_THREADS_MUTEXES)
	{
		if (hold) {
			k_mutex_lock(&rac_api_mutex, K_FOREVER);
			rzi_radio_gap_quiesce();
		}
		ignore_result(rzi_power_sleep(timeout_ms));
		if (hold) {
			lora_transceiver_board_enable_interrupt(DEVICE_DT_GET(LORA_NODE));
			k_mutex_unlock(&rac_api_mutex);
		}
	}
#elif defined(CONFIG_SOC_SERIES_NRF52)
	rzi_radio_gap_quiesce();
	ignore_result(rzi_power_sleep(timeout_ms));
#else
	ignore_result(rzi_power_sleep(timeout_ms));
#endif
	radio_bus_resume();
	console_hold();
}

/* Highest byte first, so a server reads the counter as a normal integer. */
static void encode_uplink_counter(uint32_t counter, uint8_t payload[4])
{
	payload[0] = (uint8_t)(counter >> 24);
	payload[1] = (uint8_t)(counter >> 16);
	payload[2] = (uint8_t)(counter >> 8);
	payload[3] = (uint8_t)counter;
}

int main(void)
{
	static const uint8_t dev_eui[] = DT_PROP(USER_NODE, user_lorawan_device_eui);
	static const uint8_t join_eui[] = DT_PROP(USER_NODE, user_lorawan_join_eui);
	static const uint8_t app_key[] = DT_PROP(USER_NODE, user_lorawan_app_key);
	static const uint8_t gen_app_key[] = DT_PROP(USER_NODE, user_lorawan_gen_app_key);
	uint32_t uplink_counter = 0;
	int rc;

#if defined(CONFIG_SOC_SERIES_NRF52)
	nrf52_system_on_warm_sleep();
#endif
#if defined(CONFIG_SOC_SERIES_NRF52)
	nrf52_release_unused_peripherals();
#endif
	console_uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	console_hold();

#if defined(CONFIG_SOC_SERIES_NRF52)
	{
		nrf_clock_lfclk_t lf_src = NRF_CLOCK_LFCLK_RC;
		bool lf_running = nrf_clock_is_running(NRF_CLOCK, NRF_CLOCK_DOMAIN_LFCLK, &lf_src);

		LOG_INF("LFCLK %s, source %s", lf_running ? "running" : "stopped",
			lf_src == NRF_CLOCK_LFCLK_XTAL ? "xtal" :
			lf_src == NRF_CLOCK_LFCLK_SYNTH ? "synth" : "rc");
	}
#endif
	LOG_INF("RZI LoRaWAN low power (%d s) sample", UPLINK_INTERVAL_MS / 1000);
	rc = rzi_init();
	if (rc != 0) {
		LOG_ERR("Init failed: %s", rzi_err_str(rc));
		return rc;
	}
	/* Suspend lets the idle thread enter low power between radio events. */
	rc = rzi_power_set_policy(RZI_POWER_POLICY_SUSPEND);
	if (rc != 0) {
		LOG_ERR("Power policy failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_register_callbacks(&callbacks);
	if (rc != 0) {
		LOG_ERR("Callback registration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_set_dev_eui(dev_eui, sizeof(dev_eui));
	if (rc == 0) {
		rc = rzi_lorawan_set_app_eui(join_eui, sizeof(join_eui));
	}
	if (rc == 0) {
		rc = rzi_lorawan_set_app_key(app_key, sizeof(app_key));
	}
	if (rc == 0) {
		rc = rzi_lorawan_set_gen_app_key(gen_app_key, sizeof(gen_app_key));
	}
	if (rc == 0) {
		rc = rzi_lorawan_set_activation(RZI_LORAWAN_ACTIVATION_OTAA);
	}
	if (rc != 0) {
		LOG_ERR("Credentials failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_init(LORAWAN_REGION);
	if (rc != 0) {
		LOG_ERR("LoRaWAN init failed: %s", rzi_err_str(rc));
		return rc;
	}

	k_sem_take(&stack_ready, K_FOREVER);

	/* Join and the periodic uplink are paused so the sleep current can be
	 * measured with the modem idle. The radio is warm-slept inside the gap.
	 */
	printk("join paused, sleeping\n");
#if defined(CONFIG_SOC_SERIES_NRF52)
	/* k_sleep() leaves interrupts on, so USB, RNG or the clock driver
	 * starts the 32 MHz crystal again and the floor stays near 800 µA.
	 * The 15 µA measurement stopped the crystal and then waited in WFE
	 * with interrupts masked.
	 */
	k_mutex_lock(&rac_api_mutex, K_FOREVER);
	rzi_radio_gap_quiesce();
	{
		volatile uint32_t *power = (volatile uint32_t *)(NRF_SPIM1_BASE + 0xFFCU);

		*power = 0U;
		(void)*power;
		*power = 1U;
	}
	NRF_GPIOTE->INTENCLR = 0xFFFFFFFFU;
	for (int ch = 0; ch < 8; ch++) {
		NRF_GPIOTE->CONFIG[ch] = 0U;
	}
	{
		unsigned int key = irq_lock();

		for (int n = 0; n < 1000 && (NRF_CLOCK->HFCLKSTAT & CLOCK_HFCLKSTAT_SRC_Msk) != 0U;
		     n++) {
			*(volatile uint32_t *)0x4006EC00 = 0x00009375U;
			*(volatile uint32_t *)0x4006ED14 = 0U;
			nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTOP);
		}
		for (;;) {
			__WFE();
		}
		irq_unlock(key);
	}
#else
	for (;;) {
		idle_with_console_off(60000);
	}
#endif

	/* Must run before join; LBM rejects this after JOINING/JOINED. */
	rc = pin_join_data_rate();
	if (rc != 0) {
		LOG_WRN("Join data rate not pinned: %s", rzi_err_str(rc));
	}

	while (request_join() != 0) {
		ignore_result(rzi_lorawan_leave());
		idle_with_console_off(RETRY_DELAY_MS);
	}
	LOG_INF("Joined");
	configure_after_join();

	for (;;) {
		uint8_t payload[sizeof(uplink_counter)];
		int32_t wait_ms = duty_cycle_wait_ms();
		bool sent = false;

		if (wait_ms > 0) {
			idle_with_console_off(wait_ms);
			continue;
		}

		encode_uplink_counter(uplink_counter, payload);
		k_sem_reset(&tx_done);
		rc = rzi_lorawan_send(UPLINK_PORT, payload, sizeof(payload),
				      RZI_LORAWAN_MSG_UNCONFIRMED);
		if (rzi_console_listening()) {
			printk("send t=%d rc=%d\n", k_uptime_get_32(), rc);
		}
		if (rc == 0) {
			LOG_INF("Uplink %u on port %d", uplink_counter, UPLINK_PORT);
			uplink_counter++;
			sent = true;
		} else if (rc == -RZI_ERR_BUSY) {
			LOG_INF("Radio busy, waiting");
		} else {
			LOG_WRN("Uplink rejected: %s", rzi_err_str(rc));
			report_events(false);
			idle_with_console_off(UPLINK_INTERVAL_MS);
			continue;
		}
		/* TX_DONE is after RX1/RX2. MAC answers also complete with TX_DONE. */
		console_release();
		rc = k_sem_take(&tx_done, K_MSEC(TX_DONE_TIMEOUT_MS));
		console_hold();
		if (rzi_console_listening()) {
			printk("txdone t=%d rc=%d sent=%d\n", k_uptime_get_32(), rc, sent);
		}
		if (rc != 0) {
			LOG_WRN("No TX_DONE within %d s", TX_DONE_TIMEOUT_MS / 1000);
		}
		report_events(rc == 0 && sent);
		if (!sent) {
			continue;
		}
		/* Fixed gap from the completed uplink to the next request. */
		idle_with_console_off(UPLINK_INTERVAL_MS);
	}
}
