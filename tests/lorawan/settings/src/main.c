/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/ztest.h>
#include <rzi/lorawan/lorawan.h>
#include <rzi/storage/storage.h>
#include <string.h>
#include "lorawan_priv.h"

static bool fail_write;
static bool fail_delete;
static unsigned int reads;
static unsigned int writes;
int __real_rzi_storage_read(const char *ns, const char *key, void *value, size_t size);
int __real_rzi_storage_delete(const char *ns, const char *key);
int __real_rzi_storage_write_record(const char *ns, const char *key, uint16_t version,
				    const void *value, size_t size);
int __real_rzi_storage_read_record(const char *ns, const char *key, uint16_t *version, void *value,
				   size_t capacity, size_t *size);

int __wrap_rzi_storage_read(const char *ns, const char *key, void *value, size_t size)
{
	reads++;
	return __real_rzi_storage_read(ns, key, value, size);
}

int __wrap_rzi_storage_write_record(const char *ns, const char *key, uint16_t version,
				    const void *value, size_t size)
{
	writes++;
	return fail_write ? -RZI_ERR_IO
			  : __real_rzi_storage_write_record(ns, key, version, value, size);
}

int __wrap_rzi_storage_read_record(const char *ns, const char *key, uint16_t *version, void *value,
				   size_t capacity, size_t *size)
{
	reads++;
	return __real_rzi_storage_read_record(ns, key, version, value, capacity, size);
}

int __wrap_rzi_storage_delete(const char *ns, const char *key)
{
	return fail_delete ? -RZI_ERR_IO : __real_rzi_storage_delete(ns, key);
}

int __real_rzi_storage_write(const char *ns, const char *key, const void *value, size_t size);

int __wrap_rzi_storage_write(const char *ns, const char *key, const void *value, size_t size)
{
	writes++;
	return fail_write ? -RZI_ERR_IO : __real_rzi_storage_write(ns, key, value, size);
}

static void test_credential_failure_and_policy_transaction(void)
{
	uint8_t old_key[16];
	uint8_t new_key[16];
	uint8_t actual[16];
	uint8_t old_eui[8];
	uint8_t new_eui[8];
	enum rzi_lorawan_activation mode;
	uint32_t addr;
	uint8_t interval;
	uint8_t attempts;
	bool auto_join;
	uint8_t policy[3];
	uint8_t before[15];
	uint8_t after[15];
	uint16_t version;
	size_t size;

	memset(old_key, 0x11, sizeof(old_key));
	memset(new_key, 0x22, sizeof(new_key));
	memset(old_eui, 0x33, sizeof(old_eui));
	memset(new_eui, 0x44, sizeof(new_eui));
	zassert_ok(rzi_lorawan_set_dev_eui(old_eui, sizeof(old_eui)));
	zassert_ok(rzi_lorawan_set_app_eui(old_eui, sizeof(old_eui)));
	zassert_ok(rzi_lorawan_set_app_key(old_key, sizeof(old_key)));
	zassert_ok(rzi_lorawan_set_gen_app_key(old_key, sizeof(old_key)));
	zassert_ok(rzi_lorawan_set_nwk_skey(old_key, sizeof(old_key)));
	zassert_ok(rzi_lorawan_set_app_skey(old_key, sizeof(old_key)));
	zassert_ok(rzi_lorawan_set_dev_addr(0x01020304));
	zassert_ok(rzi_lorawan_set_activation(RZI_LORAWAN_ACTIVATION_ABP));
	zassert_ok(rzi_lorawan_credentials_update_policy(1, 9, 3));
	zassert_ok(rzi_storage_read("lorawan", "join_policy", before, sizeof(before)));
	fail_write = true;
	zassert_equal(rzi_lorawan_set_dev_eui(new_eui, sizeof(new_eui)), -RZI_ERR_IO);
	zassert_equal(rzi_lorawan_set_app_eui(new_eui, sizeof(new_eui)), -RZI_ERR_IO);
	zassert_equal(rzi_lorawan_set_app_key(new_key, sizeof(new_key)), -RZI_ERR_IO);
	zassert_equal(rzi_lorawan_set_gen_app_key(new_key, sizeof(new_key)), -RZI_ERR_IO);
	zassert_equal(rzi_lorawan_set_nwk_skey(new_key, sizeof(new_key)), -RZI_ERR_IO);
	zassert_equal(rzi_lorawan_set_app_skey(new_key, sizeof(new_key)), -RZI_ERR_IO);
	zassert_equal(rzi_lorawan_set_dev_addr(0x11223344), -RZI_ERR_IO);
	zassert_equal(rzi_lorawan_set_activation(RZI_LORAWAN_ACTIVATION_OTAA), -RZI_ERR_IO);
	zassert_equal(rzi_lorawan_credentials_update_policy(0, 20, 8), -RZI_ERR_IO);
	fail_write = false;
	zassert_ok(rzi_lorawan_get_dev_eui(actual, sizeof(old_eui)));
	zassert_mem_equal(actual, old_eui, sizeof(old_eui));
	zassert_ok(rzi_lorawan_get_app_eui(actual, sizeof(old_eui)));
	zassert_mem_equal(actual, old_eui, sizeof(old_eui));
	zassert_ok(rzi_lorawan_get_app_key(actual, sizeof(actual)));
	zassert_mem_equal(actual, old_key, sizeof(actual));
	zassert_ok(rzi_lorawan_get_gen_app_key(actual, sizeof(actual)));
	zassert_mem_equal(actual, old_key, sizeof(actual));
	zassert_ok(rzi_lorawan_get_nwk_skey(actual, sizeof(actual)));
	zassert_mem_equal(actual, old_key, sizeof(actual));
	zassert_ok(rzi_lorawan_get_app_skey(actual, sizeof(actual)));
	zassert_mem_equal(actual, old_key, sizeof(actual));
	zassert_ok(rzi_lorawan_get_dev_addr(&addr));
	zassert_equal(addr, 0x01020304);
	zassert_ok(rzi_lorawan_get_activation(&mode));
	zassert_equal(mode, RZI_LORAWAN_ACTIVATION_ABP);
	zassert_ok(rzi_lorawan_get_auto_join(&auto_join));
	zassert_true(auto_join);
	zassert_ok(rzi_lorawan_get_join_interval(&interval));
	zassert_equal(interval, 9);
	zassert_ok(rzi_lorawan_get_join_attempts(&attempts));
	zassert_equal(attempts, 3);
	zassert_ok(rzi_storage_read("lorawan", "join_policy", after, sizeof(after)));
	zassert_mem_equal(before, after, sizeof(before));
	zassert_ok(rzi_lorawan_credentials_update_policy(0, 20, 8));
	zassert_ok(rzi_storage_read_record("lorawan", "join_policy", &version, policy,
					   sizeof(policy), &size));
	zassert_equal(version, 1);
	zassert_equal(size, 3);
	zassert_equal(policy[0], 0);
	zassert_equal(policy[1], 20);
	zassert_equal(policy[2], 8);
}

static void test_channel_default_and_failed_clear(void)
{
	uint16_t mask[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {1};
	uint16_t actual[RZI_LORAWAN_CHANNEL_MASK_WORDS];
	bool set;

	/* An explicit default must mask an older legacy restriction. */
	zassert_ok(rzi_storage_write("rzi", "chmask", mask, sizeof(mask)));
	zassert_ok(rzi_lorawan_credentials_set_channel_mask(mask));
	fail_write = true;
	zassert_equal(rzi_lorawan_credentials_set_channel_mask(NULL), -RZI_ERR_IO);
	fail_write = false;
	zassert_ok(rzi_lorawan_credentials_get_channel_mask(actual, &set));
	zassert_true(set);
	zassert_equal(actual[0], 1);
	zassert_ok(rzi_lorawan_credentials_set_channel_mask(NULL));
	zassert_ok(rzi_storage_read("lorawan", "chmask", actual, sizeof(actual)));
	zassert_equal(actual[0], 0);
	fail_delete = true;
	zassert_equal(rzi_lorawan_clear_settings(), -RZI_ERR_IO);
	fail_delete = false;
	zassert_ok(rzi_lorawan_clear_settings());
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
		if (marker == 2U) {
			bool auto_join;
			uint8_t interval;
			uint8_t attempts;

			zassert_ok(rzi_lorawan_get_auto_join(&auto_join));
			zassert_true(auto_join);
			zassert_ok(rzi_lorawan_get_join_interval(&interval));
			zassert_equal(interval, 30);
			zassert_ok(rzi_lorawan_get_join_attempts(&attempts));
			zassert_equal(attempts, 4);
		}
		TC_PRINT("Restored public API settings after process restart\n");
	} else {
		zassert_equal(rc, -RZI_ERR_NOT_FOUND);
	}
	zassert_ok(rzi_lorawan_clear_settings());
	zassert_ok(rzi_storage_write("rzi", "band", &band, sizeof(band)));
	zassert_ok(rzi_storage_write("rzi", "cfm", &legacy_confirmed, sizeof(legacy_confirmed)));
	zassert_ok(rzi_storage_write("rzi", "rety", &legacy_retries, sizeof(legacy_retries)));
	zassert_ok(rzi_storage_write("rzi", "netid", &legacy_net_id, sizeof(legacy_net_id)));
	{
		uint8_t auto_join = 1;
		uint8_t interval = 9;
		uint8_t attempts = 3;
		uint8_t policy[3];
		uint16_t version;
		size_t size;
		bool enabled;

		zassert_ok(rzi_storage_write("lorawan", "autojoin", &auto_join, 1));
		zassert_ok(rzi_storage_write("rzi", "join_interval", &interval, 1));
		zassert_ok(rzi_storage_write("rzi", "join_attempts", &attempts, 1));
		zassert_ok(rzi_lorawan_get_auto_join(&enabled));
		zassert_true(enabled);
		zassert_ok(rzi_storage_read_record("lorawan", "join_policy", &version, policy,
						   sizeof(policy), &size));
		zassert_equal(version, 1);
		zassert_equal(size, 3);
		zassert_equal(policy[0], 1);
		zassert_equal(policy[1], 9);
		zassert_equal(policy[2], 3);
	}
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

	{
		unsigned int read_count = reads;
		unsigned int write_count = writes;

		zassert_ok(rzi_lorawan_get_region(&region));
		zassert_ok(rzi_lorawan_get_confirmed(&confirmed));
		zassert_ok(rzi_lorawan_get_retries(&retries));
		zassert_ok(rzi_lorawan_get_net_id(&net_id));
		zassert_equal(reads, read_count, "cached getters accessed storage");
		zassert_ok(rzi_lorawan_set_retries(retries));
		zassert_ok(rzi_lorawan_set_confirmed(confirmed));
		zassert_equal(writes, write_count, "unchanged setters wrote storage");
	}
	test_credential_failure_and_policy_transaction();
	test_channel_default_and_failed_clear();
	{
		uint8_t policy[] = {1, 9, 3};
		bool enabled = false;
		uint8_t invalid = 2;

		zassert_ok(rzi_storage_write_record("lorawan", "join_policy", 2, policy, 3));
		zassert_equal(rzi_lorawan_get_auto_join(&enabled), -RZI_ERR_NOT_SUPPORTED);
		zassert_false(enabled);
		/* A failed load must be retryable after the unsupported data is repaired. */
		zassert_ok(rzi_storage_write_record("lorawan", "join_policy", 1, policy, 3));
		zassert_ok(rzi_lorawan_get_auto_join(&enabled));
		zassert_true(enabled);
		zassert_ok(rzi_storage_write("lorawan", "cfm", &invalid, 1));
		zassert_equal(rzi_lorawan_get_confirmed(&enabled), -RZI_ERR_BAD_MESSAGE);
		zassert_ok(rzi_lorawan_clear_settings());
		zassert_ok(rzi_lorawan_get_confirmed(&enabled));
		zassert_false(enabled);
	}

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
	marker = 2;
	zassert_ok(rzi_storage_write("test", "boot", &marker, sizeof(marker)));
	zassert_ok(rzi_lorawan_credentials_update_policy(1, 30, 4));
	verify_persistent_values();
}

ZTEST_SUITE(rzi_lorawan_settings, NULL, NULL, NULL, NULL, NULL);
