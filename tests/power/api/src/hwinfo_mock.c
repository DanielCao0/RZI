/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/drivers/hwinfo.h>

uint32_t mock_reset_flags = RESET_PIN | RESET_WATCHDOG;
int mock_reset_error;

int z_impl_hwinfo_get_reset_cause(uint32_t *cause)
{
	if (mock_reset_error != 0) {
		return mock_reset_error;
	}
	*cause = mock_reset_flags;
	return 0;
}

int z_impl_hwinfo_clear_reset_cause(void)
{
	if (mock_reset_error != 0) {
		return mock_reset_error;
	}
	mock_reset_flags = 0;
	return 0;
}
