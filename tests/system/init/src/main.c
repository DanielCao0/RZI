/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <zephyr/ztest.h>

#include <rzi/err.h>
#include <rzi/init.h>
#include <rzi/power/power.h>
#include <rzi/storage/storage.h>

ZTEST(rzi_system_init, test_init_prepares_compiled_services)
{
	bool enabled;
	uint8_t value = 0U;

	zassert_ok(rzi_init());
	zassert_ok(rzi_init());
	zassert_ok(rzi_power_get_pm_enabled(&enabled));
	zassert_equal(enabled, IS_ENABLED(CONFIG_PM));
	zassert_equal(rzi_storage_read("rzi", "missing", &value, sizeof(value)),
		      -RZI_ERR_NOT_FOUND);
}

ZTEST_SUITE(rzi_system_init, NULL, NULL, NULL, NULL, NULL);
