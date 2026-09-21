/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include <rzi/err.h>

ZTEST(rzi_err_catalog, test_str_covers_closed_catalog)
{
	static const struct {
		int value;
		const char *name;
	} cases[] = {
		{RZI_SUCCESS, "RZI_SUCCESS"},
		{-RZI_ERR_INVALID, "RZI_ERR_INVALID"},
		{RZI_ERR_WOULDBLOCK, "RZI_ERR_WOULDBLOCK"},
		{-RZI_ERR_NOT_READY, "RZI_ERR_NOT_READY"},
		{RZI_ERR_ALREADY, "RZI_ERR_ALREADY"},
		{-RZI_ERR_BUSY, "RZI_ERR_BUSY"},
		{RZI_ERR_NOT_SUPPORTED, "RZI_ERR_NOT_SUPPORTED"},
		{-RZI_ERR_NO_RESOURCE, "RZI_ERR_NO_RESOURCE"},
		{RZI_ERR_NOT_FOUND, "RZI_ERR_NOT_FOUND"},
		{-RZI_ERR_NO_DATA, "RZI_ERR_NO_DATA"},
		{RZI_ERR_TOO_LARGE, "RZI_ERR_TOO_LARGE"},
		{-RZI_ERR_TIMEOUT, "RZI_ERR_TIMEOUT"},
		{RZI_ERR_OVERFLOW, "RZI_ERR_OVERFLOW"},
		{-RZI_ERR_IO, "RZI_ERR_IO"},
		{RZI_ERR_BAD_MESSAGE, "RZI_ERR_BAD_MESSAGE"},
		{-RZI_ERR_NO_DEVICE, "RZI_ERR_NO_DEVICE"},
		{RZI_ERR_NOT_JOINED, "RZI_ERR_NOT_JOINED"},
		{-RZI_ERR_DENIED, "RZI_ERR_DENIED"},
		{99, "RZI_ERR_UNKNOWN"},
	};

	for (size_t i = 0; i < ARRAY_SIZE(cases); ++i) {
		zassert_ok(strcmp(rzi_err_str(cases[i].value), cases[i].name), "value %d",
			   cases[i].value);
	}
}

ZTEST(rzi_err_catalog, test_errno_mapper_preserves_precision)
{
	zassert_equal(rzi_err_from_errno(0), 0);
	zassert_equal(rzi_err_from_errno(1), 0);
	zassert_equal(rzi_err_from_errno(-EINVAL), -RZI_ERR_INVALID);
	zassert_equal(rzi_err_from_errno(-EWOULDBLOCK), -RZI_ERR_NOT_READY);
	zassert_equal(rzi_err_from_errno(-EALREADY), -RZI_ERR_ALREADY);
	zassert_equal(rzi_err_from_errno(-EEXIST), -RZI_ERR_ALREADY);
	zassert_equal(rzi_err_from_errno(-EBUSY), -RZI_ERR_BUSY);
	zassert_equal(rzi_err_from_errno(-ENOTSUP), -RZI_ERR_NOT_SUPPORTED);
	zassert_equal(rzi_err_from_errno(-ENOSYS), -RZI_ERR_NOT_SUPPORTED);
	zassert_equal(rzi_err_from_errno(-ENOMEM), -RZI_ERR_NO_RESOURCE);
	zassert_equal(rzi_err_from_errno(-ENOSPC), -RZI_ERR_NO_RESOURCE);
	zassert_equal(rzi_err_from_errno(-ENOENT), -RZI_ERR_NOT_FOUND);
	zassert_equal(rzi_err_from_errno(-ENODATA), -RZI_ERR_NO_DATA);
	zassert_equal(rzi_err_from_errno(-EMSGSIZE), -RZI_ERR_TOO_LARGE);
	zassert_equal(rzi_err_from_errno(-ETIMEDOUT), -RZI_ERR_TIMEOUT);
	zassert_equal(rzi_err_from_errno(-EOVERFLOW), -RZI_ERR_OVERFLOW);
	zassert_equal(rzi_err_from_errno(-EBADMSG), -RZI_ERR_BAD_MESSAGE);
	zassert_equal(rzi_err_from_errno(-ENODEV), -RZI_ERR_NO_DEVICE);
	zassert_equal(rzi_err_from_errno(-ENETDOWN), -RZI_ERR_NOT_JOINED);
	zassert_equal(rzi_err_from_errno(-EACCES), -RZI_ERR_DENIED);
	zassert_equal(rzi_err_from_errno(-EPERM), -RZI_ERR_DENIED);
	zassert_equal(rzi_err_from_errno(-EIO), -RZI_ERR_IO);
	zassert_equal(rzi_err_from_errno(-12345), -RZI_ERR_IO);
}

ZTEST_SUITE(rzi_err_catalog, NULL, NULL, NULL, NULL, NULL);
