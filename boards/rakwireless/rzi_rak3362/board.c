/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 RAKwireless Technology Limited
 */

#include <zephyr/init.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/sys/util_macro.h>
#include <zephyr/dt-bindings/pinctrl/nrf-pinctrl.h>

#define WISBLOCK_PWM_MARK(node, ctlr, ch)						\
	COND_CODE_1(DT_NODE_HAS_PROP(node, pwms),					\
		(COND_CODE_1(DT_SAME_NODE(DT_PWMS_CTLR_BY_IDX(node, 0), ctlr),		\
			(COND_CODE_1(IS_EQ(DT_PWMS_CHANNEL_BY_IDX(node, 0), ch),	\
				(x), ())),						\
			())),								\
		())

#define WISBLOCK_PWM_ACTIVE(ctlr, ch)							\
	UTIL_NOT(IS_EMPTY(DT_FOREACH_STATUS_OKAY_NODE_VARGS(WISBLOCK_PWM_MARK, ctlr, ch)))

#define WISBLOCK_PWM_OUT(ctlr, ch, fun, port, pin)					\
	COND_CODE_1(WISBLOCK_PWM_ACTIVE(DT_NODELABEL(ctlr), ch),			\
		(NRF_PSEL(fun, port, pin)), (NRF_PSEL_DISCONNECTED(fun)))

#define WISBLOCK_PWM_STATES(name)							\
	PINCTRL_DT_DEV_CONFIG_DECLARE(DT_NODELABEL(name));				\
	static const struct pinctrl_state name##_states[] = {				\
		{.pins = name##_pins, .pin_cnt = ARRAY_SIZE(name##_pins),		\
		 .id = PINCTRL_STATE_DEFAULT},						\
		{.pins = name##_sleep, .pin_cnt = ARRAY_SIZE(name##_sleep),		\
		 .id = PINCTRL_STATE_SLEEP},						\
	}

#define WISBLOCK_PWM_UPDATE(name)							\
	(void)pinctrl_update_states(PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(name)),	\
				    name##_states, ARRAY_SIZE(name##_states))

static const pinctrl_soc_pin_t pwm20_pins[] = {
	WISBLOCK_PWM_OUT(pwm20, 0, PWM_OUT0, 1, 14),
	WISBLOCK_PWM_OUT(pwm20, 1, PWM_OUT1, 2, 0),
};
static const pinctrl_soc_pin_t pwm21_pins[] = {
	WISBLOCK_PWM_OUT(pwm21, 0, PWM_OUT0, 2, 3),
	WISBLOCK_PWM_OUT(pwm21, 1, PWM_OUT1, 2, 6),
};
static const pinctrl_soc_pin_t pwm22_pins[] = {
	WISBLOCK_PWM_OUT(pwm22, 0, PWM_OUT0, 1, 2),
	WISBLOCK_PWM_OUT(pwm22, 1, PWM_OUT1, 1, 3),
	WISBLOCK_PWM_OUT(pwm22, 2, PWM_OUT2, 1, 8),
};

static const pinctrl_soc_pin_t pwm20_sleep[] = {
	NRF_PSEL_DISCONNECTED(PWM_OUT0),
	NRF_PSEL_DISCONNECTED(PWM_OUT1),
};
static const pinctrl_soc_pin_t pwm21_sleep[] = {
	NRF_PSEL_DISCONNECTED(PWM_OUT0),
	NRF_PSEL_DISCONNECTED(PWM_OUT1),
};
static const pinctrl_soc_pin_t pwm22_sleep[] = {
	NRF_PSEL_DISCONNECTED(PWM_OUT0),
	NRF_PSEL_DISCONNECTED(PWM_OUT1),
	NRF_PSEL_DISCONNECTED(PWM_OUT2),
};

WISBLOCK_PWM_STATES(pwm20);
WISBLOCK_PWM_STATES(pwm21);
WISBLOCK_PWM_STATES(pwm22);

void board_early_init_hook(void)
{
	WISBLOCK_PWM_UPDATE(pwm20);
	WISBLOCK_PWM_UPDATE(pwm21);
	WISBLOCK_PWM_UPDATE(pwm22);
}
