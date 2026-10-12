/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI SDK version implementation.
 */

#include <rzi/version.h>

const char *rzi_version_get_string(void)
{
	return RZI_VERSION_STRING;
}
