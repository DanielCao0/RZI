/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI LoRaWAN MAC-command request API.
 */
#ifndef RZI_LORAWAN_MAC_COMMANDS_H
#define RZI_LORAWAN_MAC_COMMANDS_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/toolchain.h>

#include <rzi/lorawan/lorawan.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @ingroup rzi_lorawan
 *  @{
 */

/** LinkCheckReq scheduling policy. */
enum rzi_lorawan_link_check_mode {
	/** Do not append LinkCheckReq. */
	RZI_LORAWAN_LINK_CHECK_DISABLED,
	/** Append LinkCheckReq to the next uplink only. */
	RZI_LORAWAN_LINK_CHECK_ONCE,
	/** Append LinkCheckReq to every uplink. */
	RZI_LORAWAN_LINK_CHECK_EVERY_UPLINK,
};

/** Network time from DeviceTimeAns, GPS epoch. */
struct rzi_lorawan_network_time {
	/** Whole GPS seconds. */
	uint32_t gps_seconds;
	/** Fractional GPS seconds as reported by the stack. */
	uint32_t gps_subseconds;
};

/**
 * @brief Read the current LinkCheckReq mode.
 *
 * @param[out] mode Stored mode.
 *
 * @retval 0 Value stored.
 * @retval -EINVAL mode is NULL.
 * @retval -EAGAIN The service has not started.
 * @retval -ENOTSUP The backend does not support LinkCheckReq.
 * @retval -EWOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_link_check_mode(enum rzi_lorawan_link_check_mode *mode);

/**
 * @brief Request LinkCheckReq according to mode.
 *
 * Completion is reported through on_event() as RZI_LORAWAN_EVENT_LINK_CHECK.
 * A return of zero means
 * the mode was stored. ONCE and EVERY_UPLINK trigger a request immediately;
 * EVERY_UPLINK also piggybacks on later uplinks.
 *
 * @param mode Scheduling policy.
 *
 * @retval 0 Mode stored.
 * @retval -EINVAL mode is invalid.
 * @retval -EAGAIN The service has not started.
 * @retval -ENOTSUP The backend does not support LinkCheckReq.
 * @retval -EWOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_request_link_check(enum rzi_lorawan_link_check_mode mode);

/**
 * @brief Read whether DeviceTimeReq is enabled.
 *
 * @param[out] enabled True when DeviceTimeReq is scheduled.
 *
 * @retval 0 Value stored.
 * @retval -EINVAL enabled is NULL.
 * @retval -EAGAIN The service has not started.
 * @retval -ENOTSUP The backend does not support DeviceTimeReq.
 * @retval -EWOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_device_time_enabled(bool *enabled);

/**
 * @brief Enable or disable DeviceTimeReq on subsequent uplinks.
 *
 * Completion is reported through on_event() as RZI_LORAWAN_EVENT_DEVICE_TIME.
 *
 * @param enabled True to request network time.
 *
 * @retval 0 Setting stored.
 * @retval -EAGAIN The service has not started.
 * @retval -ENOTSUP The backend does not support DeviceTimeReq.
 * @retval -EWOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_request_device_time(bool enabled);

/**
 * @brief Read the last DeviceTimeAns.
 *
 * @param[out] time Stored GPS time.
 *
 * @retval 0 Time stored.
 * @retval -EINVAL time is NULL.
 * @retval -EAGAIN The service has not started or the clock is unsynchronized.
 * @retval -ENOTSUP The backend does not support DeviceTimeReq.
 * @retval -EWOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_network_time(struct rzi_lorawan_network_time *time);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_LORAWAN_MAC_COMMANDS_H */
