/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <zephyr/ztest.h>

#include <rzi/storage/storage.h>

ZTEST(rzi_storage_nvs, test_replace_and_exact_size)
{
	uint8_t value[16];
	uint8_t actual[17];

	memset(value, 0x5a, sizeof(value));
	zassert_ok(rzi_storage_write("audit", "value", value, sizeof(value)));
	zassert_ok(rzi_storage_read("audit", "value", actual, sizeof(value)));
	zassert_mem_equal(actual, value, sizeof(value));
	zassert_equal(rzi_storage_read("audit", "value", actual, sizeof(value) - 1),
		      -RZI_ERR_TOO_LARGE);
	zassert_equal(rzi_storage_read("audit", "value", actual, sizeof(actual)),
		      -RZI_ERR_TOO_LARGE);
	value[0] = 0xa5;
	zassert_ok(rzi_storage_write("audit", "value", value, sizeof(value)));
	zassert_ok(rzi_storage_read("audit", "value", actual, sizeof(value)));
	zassert_mem_equal(actual, value, sizeof(value));
	zassert_ok(rzi_storage_delete("audit", "value"));
}

ZTEST(rzi_storage_nvs, test_delete_missing_key)
{
	const uint8_t value = 42;
	uint8_t actual;

	zassert_ok(rzi_storage_write("audit", "delete", &value, sizeof(value)));
	zassert_ok(rzi_storage_delete("audit", "delete"));
	zassert_equal(rzi_storage_read("audit", "delete", &actual, sizeof(actual)),
		      -RZI_ERR_NOT_FOUND);
	zassert_equal(rzi_storage_delete("audit", "delete"), -RZI_ERR_NOT_FOUND);
}

ZTEST(rzi_storage_nvs, test_gc_preserves_other_keys)
{
	const uint8_t stable[] = {1, 2, 3, 4};
	uint8_t value[128];
	uint8_t actual[128];

	zassert_ok(rzi_storage_write("audit", "stable", stable, sizeof(stable)));
	for (unsigned int i = 0; i < 200; ++i) {
		memset(value, (uint8_t)i, sizeof(value));
		zassert_ok(rzi_storage_write("audit", "rolling", value, sizeof(value)));
	}
	zassert_ok(rzi_storage_read("audit", "rolling", actual, sizeof(value)));
	zassert_mem_equal(actual, value, sizeof(value));
	zassert_ok(rzi_storage_read("audit", "stable", actual, sizeof(stable)));
	zassert_mem_equal(actual, stable, sizeof(stable));
	zassert_ok(rzi_storage_delete("audit", "rolling"));
	zassert_ok(rzi_storage_delete("audit", "stable"));
}

ZTEST(rzi_storage_nvs, test_persistent_value)
{
	const uint8_t expected[] = {0x52, 0x5a, 0x49, 0x31, 0x37, 0x32, 0x46, 0x01, 0x5a};
	uint8_t actual[sizeof(expected)];
	int rc = rzi_storage_read("audit", "persistent", actual, sizeof(actual));

	if (rc == -RZI_ERR_NOT_FOUND) {
		zassert_ok(rzi_storage_write("audit", "persistent", expected, sizeof(expected)));
		zassert_ok(rzi_storage_read("audit", "persistent", actual, sizeof(actual)));
		TC_PRINT("Created persistent NVS value\n");
	} else {
		zassert_ok(rc);
		TC_PRINT("Restored persistent NVS value\n");
	}
	zassert_mem_equal(actual, expected, sizeof(expected));
}

ZTEST_SUITE(rzi_storage_nvs, NULL, NULL, NULL, NULL, NULL);
