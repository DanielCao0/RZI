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

ZTEST(rzi_storage_nvs, test_versioned_records_and_corruption)
{
	uint8_t payload[] = {1, 2, 3, 4};
	uint8_t actual[4] = {0};
	uint8_t raw[16];
	uint16_t version = 99;
	size_t size = 99;

	zassert_ok(rzi_storage_write_record("record", "config", 1, payload, sizeof(payload)));
	zassert_ok(rzi_storage_read_record("record", "config", &version, actual, sizeof(actual),
					   &size));
	zassert_equal(version, 1);
	zassert_equal(size, sizeof(payload));
	zassert_mem_equal(actual, payload, sizeof(payload));
	/* The reader exposes the schema version so the application can migrate it. */
	zassert_ok(rzi_storage_write_record("record", "config", 2, payload, sizeof(payload)));
	zassert_ok(rzi_storage_read_record("record", "config", &version, actual, sizeof(actual),
					   &size));
	zassert_equal(version, 2);
	version = 99;
	size = 99;
	memset(actual, 0xa5, sizeof(actual));
	zassert_equal(rzi_storage_read_record("record", "config", &version, actual, 1, &size),
		      -RZI_ERR_TOO_LARGE);
	zassert_equal(version, 99);
	zassert_equal(size, 99);
	zassert_equal(actual[0], 0xa5);
	zassert_ok(rzi_storage_read("record", "config", raw, sizeof(raw)));
	raw[15] ^= 1; /* Simulate damaged payload with an otherwise valid header. */
	zassert_ok(rzi_storage_write("record", "config", raw, sizeof(raw)));
	zassert_equal(rzi_storage_read_record("record", "config", &version, actual, sizeof(actual),
					      &size),
		      -RZI_ERR_BAD_MESSAGE);
	zassert_equal(version, 99);
	zassert_equal(actual[0], 0xa5);
	zassert_equal(rzi_storage_write_record("record", "config", 0, payload, sizeof(payload)),
		      -RZI_ERR_INVALID);
	zassert_equal(rzi_storage_write_record("record", "config", 1, payload,
					       CONFIG_RZI_STORAGE_RECORD_MAX_SIZE + 1),
		      -RZI_ERR_TOO_LARGE);
	zassert_ok(rzi_storage_delete("record", "config"));
}

ZTEST(rzi_storage_nvs, test_capacity_and_namespace_accounting)
{
	struct rzi_storage_info info;
	struct rzi_storage_usage usage;
	uint8_t value[4] = {0};

	zassert_ok(rzi_storage_get_info(&info));
	zassert_equal(info.total_bytes, 8192);
	zassert_equal(info.sector_bytes, 2048);
	zassert_true(info.free_bytes <= info.total_bytes);
	zassert_ok(rzi_storage_write("usage", "a", value, sizeof(value)));
	zassert_ok(rzi_storage_write("usage", "nested/b", value, sizeof(value)));
	zassert_ok(rzi_storage_write("usage_other", "c", value, sizeof(value)));
	zassert_ok(rzi_storage_get_usage("usage", &usage));
	zassert_equal(usage.keys, 2);
	zassert_equal(usage.value_bytes, 8);
	zassert_equal(usage.name_bytes, strlen("a") + strlen("nested/b"));
	zassert_ok(rzi_storage_delete("usage", "a"));
	zassert_ok(rzi_storage_get_usage("usage", &usage));
	zassert_equal(usage.keys, 1);
	zassert_equal(usage.value_bytes, 4);
	zassert_ok(rzi_storage_delete("usage", "nested/b"));
	zassert_ok(rzi_storage_delete("usage_other", "c"));
}

ZTEST(rzi_storage_nvs, test_full_partition_preserves_existing_values)
{
	uint8_t payload[128];
	uint8_t actual;
	uint8_t stable = 0x5a;
	unsigned int count;
	char key[16];

	memset(payload, 0x55, sizeof(payload));
	zassert_ok(rzi_storage_write("full_test", "stable", &stable, sizeof(stable)));
	for (count = 0; count < 256U; ++count) {
		int rc;

		snprintk(key, sizeof(key), "value%u", count);
		rc = rzi_storage_write("full_test", key, payload, sizeof(payload));
		if (rc != 0) {
			zassert_equal(rc, -RZI_ERR_NO_RESOURCE);
			break;
		}
	}
	zassert_true(count < 256U, "expected the finite partition to fill");
	zassert_ok(rzi_storage_read("full_test", "stable", &actual, sizeof(actual)));
	zassert_equal(actual, stable);
	for (unsigned int i = 0; i < count; ++i) {
		snprintk(key, sizeof(key), "value%u", i);
		zassert_ok(rzi_storage_delete("full_test", key));
	}
	zassert_ok(rzi_storage_delete("full_test", "stable"));
}
