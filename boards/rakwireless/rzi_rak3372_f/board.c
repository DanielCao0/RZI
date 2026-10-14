/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>

#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>

/*
 * SYSCLK is the 32 MHz TCXO on PB0-VDD_TCXO. STOP switches that supply off,
 * and the wake path does not get HSE back before the log thread can run.
 * Stay in WFI until that restore is confirmed on hardware.
 */
static int rak3372_f_lock_stop(void)
{
	pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
	return 0;
}

SYS_INIT(rak3372_f_lock_stop, POST_KERNEL, 0);
#endif
