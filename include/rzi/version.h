/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI SDK version information.
 */
#ifndef RZI_VERSION_H
#define RZI_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

/** RZI SDK semantic-version major component. */
#define RZI_VERSION_MAJOR  0
/** RZI SDK semantic-version minor component. */
#define RZI_VERSION_MINOR  2
/** RZI SDK semantic-version patch component. */
#define RZI_VERSION_PATCH  0
/** Canonical RZI SDK semantic-version string. */
#define RZI_VERSION_STRING "0.2.0"

/** @defgroup rzi_version RZI SDK version
 *  @brief Process-wide RZI SDK version query.
 *  @since 0.2
 *  @version 0.2.0
 *  @{
 */

/**
 * @brief Return the canonical RZI SDK semantic version.
 *
 * @return Static null-terminated version string.
 * @since 0.2
 */
const char *rzi_version_get_string(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_VERSION_H */
