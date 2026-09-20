/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Closed RZI public error catalog helpers.
 */

#include <errno.h>

#include <rzi/err.h>

const char *rzi_err_str(int err)
{
	if (err < 0) {
		err = -err;
	}

	switch ((enum rzi_err)err) {
	case RZI_SUCCESS:
		return "RZI_SUCCESS";
	case RZI_ERR_INVALID:
		return "RZI_ERR_INVALID";
	case RZI_ERR_WOULDBLOCK:
		return "RZI_ERR_WOULDBLOCK";
	case RZI_ERR_NOT_READY:
		return "RZI_ERR_NOT_READY";
	case RZI_ERR_ALREADY:
		return "RZI_ERR_ALREADY";
	case RZI_ERR_BUSY:
		return "RZI_ERR_BUSY";
	case RZI_ERR_NOT_SUPPORTED:
		return "RZI_ERR_NOT_SUPPORTED";
	case RZI_ERR_NO_RESOURCE:
		return "RZI_ERR_NO_RESOURCE";
	case RZI_ERR_NOT_FOUND:
		return "RZI_ERR_NOT_FOUND";
	case RZI_ERR_NO_DATA:
		return "RZI_ERR_NO_DATA";
	case RZI_ERR_TOO_LARGE:
		return "RZI_ERR_TOO_LARGE";
	case RZI_ERR_TIMEOUT:
		return "RZI_ERR_TIMEOUT";
	case RZI_ERR_OVERFLOW:
		return "RZI_ERR_OVERFLOW";
	case RZI_ERR_IO:
		return "RZI_ERR_IO";
	case RZI_ERR_BAD_MESSAGE:
		return "RZI_ERR_BAD_MESSAGE";
	case RZI_ERR_NO_DEVICE:
		return "RZI_ERR_NO_DEVICE";
	case RZI_ERR_NOT_JOINED:
		return "RZI_ERR_NOT_JOINED";
	case RZI_ERR_DENIED:
		return "RZI_ERR_DENIED";
	}

	return "RZI_ERR_UNKNOWN";
}

int rzi_err_from_errno(int rc)
{
	if (rc >= 0) {
		return 0;
	}

	switch (rc) {
	case -EINVAL:
		return -RZI_ERR_INVALID;
	case -EWOULDBLOCK:
#if EAGAIN != EWOULDBLOCK
	case -EAGAIN:
#endif
		return -RZI_ERR_NOT_READY;
	case -EALREADY:
		return -RZI_ERR_ALREADY;
	case -EBUSY:
		return -RZI_ERR_BUSY;
	case -ENOTSUP:
#ifdef EOPNOTSUPP
#if EOPNOTSUPP != ENOTSUP
	case -EOPNOTSUPP:
#endif
#endif
		return -RZI_ERR_NOT_SUPPORTED;
	case -ENOMEM:
		return -RZI_ERR_NO_RESOURCE;
	case -ENOENT:
		return -RZI_ERR_NOT_FOUND;
	case -ENODATA:
		return -RZI_ERR_NO_DATA;
	case -EMSGSIZE:
	case -ENAMETOOLONG:
		return -RZI_ERR_TOO_LARGE;
	case -ETIMEDOUT:
		return -RZI_ERR_TIMEOUT;
	case -EOVERFLOW:
	case -ENOSPC:
		return -RZI_ERR_OVERFLOW;
	case -EBADMSG:
		return -RZI_ERR_BAD_MESSAGE;
	case -ENODEV:
		return -RZI_ERR_NO_DEVICE;
	case -ENETDOWN:
		return -RZI_ERR_NOT_JOINED;
	case -EACCES:
		return -RZI_ERR_DENIED;
	case -EIO:
	default:
		return -RZI_ERR_IO;
	}
}
