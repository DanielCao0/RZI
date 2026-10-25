/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/ztest.h>
#include <rzi/lorawan/lorawan.h>
#include <rzi/storage/storage.h>

static bool fail_write;
int __real_rzi_storage_write(const char *ns, const char *key, const void *value, size_t size);

int __wrap_rzi_storage_write(const char *ns, const char *key, const void *value, size_t size)
{
	return fail_write ? -RZI_ERR_IO : __real_rzi_storage_write(ns, key, value, size);
}

static void verify_persistent_values(void)
{
	enum rzi_lorawan_region region;
	bool confirmed;
	uint8_t retries;
	uint32_t net_id;

	zassert_ok(rzi_lorawan_get_region(&region));
	zassert_equal(region, RZI_LORAWAN_REGION_AU_915);
	zassert_ok(rzi_lorawan_get_confirmed(&confirmed));
	zassert_true(confirmed);
	zassert_ok(rzi_lorawan_get_retries(&retries));
	zassert_equal(retries, 5);
	zassert_ok(rzi_lorawan_get_net_id(&net_id));
	zassert_equal(net_id, 0x123456);
}

ZTEST(rzi_lorawan_settings, test_service_owned_settings)
{
	enum rzi_lorawan_region region;
	bool confirmed;
	uint8_t retries;
	uint32_t net_id;
	uint8_t stored;
	uint8_t marker;
	uint8_t band = 5; /* Legacy AT US915 numbering. */
	uint8_t legacy_confirmed = 1;
	uint8_t legacy_retries = 3;
	uint32_t legacy_net_id = 0x010203;

	/* A second process using the same flash file must restore without AT. */
	int rc = rzi_storage_read("test", "boot", &marker, sizeof(marker));

	if (rc == 0) {
		verify_persistent_values();
		TC_PRINT("Restored public API settings after process restart\n");
	} else {
		zassert_equal(rc, -RZI_ERR_NOT_FOUND);
	}
	zassert_ok(rzi_lorawan_clear_settings());
	zassert_ok(rzi_storage_write("rzi", "band", &band, sizeof(band)));
	zassert_ok(rzi_storage_write("rzi", "cfm", &legacy_confirmed, sizeof(legacy_confirmed)));
	zassert_ok(rzi_storage_write("rzi", "rety", &legacy_retries, sizeof(legacy_retries)));
	zassert_ok(rzi_storage_write("rzi", "netid", &legacy_net_id, sizeof(legacy_net_id)));
	zassert_ok(rzi_lorawan_get_region(&region));
	zassert_equal(region, RZI_LORAWAN_REGION_US_915);
	zassert_ok(rzi_lorawan_get_confirmed(&confirmed));
	zassert_true(confirmed);
	zassert_ok(rzi_lorawan_get_retries(&retries));
	zassert_equal(retries, 3);
	zassert_ok(rzi_lorawan_get_net_id(&net_id));
	zassert_equal(net_id, legacy_net_id);
	zassert_ok(rzi_storage_read("lorawan", "region", &stored, sizeof(stored)));
	zassert_equal(stored, RZI_LORAWAN_REGION_US_915);

	/* New API writes must take precedence over stale legacy AT values. */
	zassert_ok(rzi_lorawan_set_region(RZI_LORAWAN_REGION_AU_915));
	zassert_ok(rzi_lorawan_set_confirmed(false));
	zassert_ok(rzi_lorawan_set_retries(5));
	zassert_ok(rzi_lorawan_set_net_id(0x123456));
	zassert_ok(rzi_lorawan_get_confirmed(&confirmed));
	zassert_false(confirmed);

	fail_write = true;
	zassert_equal(rzi_lorawan_set_confirmed(true), -RZI_ERR_IO);
	zassert_equal(rzi_lorawan_set_retries(2), -RZI_ERR_IO);
	zassert_equal(rzi_lorawan_set_net_id(0x654321), -RZI_ERR_IO);
	zassert_equal(rzi_lorawan_set_region(RZI_LORAWAN_REGION_EU_868), -RZI_ERR_IO);
	fail_write = false;
	zassert_ok(rzi_lorawan_get_confirmed(&confirmed));
	zassert_false(confirmed);
	zassert_ok(rzi_lorawan_get_retries(&retries));
	zassert_equal(retries, 5);
	zassert_ok(rzi_lorawan_get_net_id(&net_id));
	zassert_equal(net_id, 0x123456);
	zassert_ok(rzi_lorawan_get_region(&region));
	zassert_equal(region, RZI_LORAWAN_REGION_AU_915);
	zassert_equal(rzi_lorawan_set_retries(8), -RZI_ERR_INVALID);
	zassert_equal(rzi_lorawan_set_net_id(0x1000000), -RZI_ERR_INVALID);
	zassert_equal(rzi_lorawan_get_confirmed(NULL), -RZI_ERR_INVALID);
	zassert_equal(rzi_lorawan_get_retries(NULL), -RZI_ERR_INVALID);
	zassert_equal(rzi_lorawan_get_net_id(NULL), -RZI_ERR_INVALID);

	/* Clearing must remove BOTH namespaces, preventing stale-value resurrection. */
	zassert_ok(rzi_lorawan_clear_settings());
	zassert_equal(rzi_storage_read("rzi", "band", &stored, sizeof(stored)), -RZI_ERR_NOT_FOUND);
	zassert_equal(rzi_storage_read("rzi", "cfm", &stored, sizeof(stored)), -RZI_ERR_NOT_FOUND);
	zassert_equal(rzi_storage_read("lorawan", "cfm", &stored, sizeof(stored)),
		      -RZI_ERR_NOT_FOUND);

	zassert_ok(rzi_lorawan_set_region(RZI_LORAWAN_REGION_AU_915));
	zassert_ok(rzi_lorawan_set_confirmed(true));
	zassert_ok(rzi_lorawan_set_retries(5));
	zassert_ok(rzi_lorawan_set_net_id(0x123456));
	marker = 1;
	zassert_ok(rzi_storage_write("test", "boot", &marker, sizeof(marker)));
	verify_persistent_values();
}

ZTEST_SUITE(rzi_lorawan_settings, NULL, NULL, NULL, NULL, NULL);
