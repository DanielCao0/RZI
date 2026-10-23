/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Power-off entry for SoCs whose Zephyr port owns the registers.
 *
 * STM32WLE5 STOP is selected by the idle thread from devicetree power
 * states. STANDBY is sys_poweroff().
 */

#include <rzi/err.h>

#ifdef CONFIG_POWEROFF
#include <zephyr/sys/poweroff.h>
#endif

#include "power_board.h"

int power_board_prepare_retention(void)
{
	return 0;
}

int power_board_enter_off(void)
{
#ifdef CONFIG_POWEROFF
	sys_poweroff();
#endif
	return -RZI_ERR_NOT_SUPPORTED;
}
