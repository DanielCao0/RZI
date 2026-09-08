/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <rzi/capabilities.h>
#include <rzi/storage.h>

ZTEST(rzi_storage_api, test_initialization_and_capability)
{
	zassert_ok(rzi_storage_init());
	zassert_equal(rzi_get_capabilities(), RZI_CAP_STORAGE);
}

ZTEST(rzi_storage_api, test_arguments_are_validated_before_backend_access)
{
	char long_key[80];
	uint8_t value = 0;

	memset(long_key, 'a', sizeof(long_key) - 1U);
	long_key[sizeof(long_key) - 1U] = '\0';
	zassert_equal(rzi_storage_read(NULL, "key", &value, sizeof(value)), -EINVAL);
	zassert_equal(rzi_storage_read("rzi", "", &value, sizeof(value)), -EINVAL);
	zassert_equal(rzi_storage_read("rzi", "key", NULL, sizeof(value)), -EINVAL);
	zassert_equal(rzi_storage_write("rzi", "key", NULL, sizeof(value)), -EINVAL);
	zassert_equal(rzi_storage_delete("rzi", long_key), -ENAMETOOLONG);
	zassert_equal(rzi_storage_read("rzi", "missing", &value, sizeof(value)), -ENOENT);
}

ZTEST_SUITE(rzi_storage_api, NULL, NULL, NULL, NULL, NULL);
