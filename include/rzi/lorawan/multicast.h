/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI LoRaWAN multicast-session API.
 */
#ifndef RZI_LORAWAN_MULTICAST_H
#define RZI_LORAWAN_MULTICAST_H

#include <stddef.h>
#include <stdint.h>
#include <zephyr/toolchain.h>

#include <rzi/lorawan/lorawan.h>
#include <rzi/err.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @ingroup rzi_lorawan
 *  @{
 */

/** One multicast session. Keys are copied before add returns. */
struct rzi_lorawan_multicast_session {
	/** Class B or Class C receive class. */
	enum rzi_lorawan_class device_class;
	/** Multicast device address in host byte order. */
	uint32_t dev_addr;
	/** Multicast application session key. */
	uint8_t application_session_key[16];
	/** Multicast network session key. */
	uint8_t network_session_key[16];
	/** Downlink frequency in hertz. */
	uint32_t frequency_hz;
	/** Downlink data rate. */
	enum rzi_lorawan_data_rate data_rate;
	/** Class B ping-slot periodicity, ignored for Class C. */
	uint16_t periodicity;
	/** Group identifier in the range 0 through 3, or -1 to allocate. */
	int8_t group_id;
};

/**
 * @brief Add and start one multicast session.
 *
 * @param session Session parameters copied before this function returns.
 *
 * @retval 0 Session started.
 * @retval -RZI_ERR_INVALID session is NULL or contains an invalid class, group, or data rate.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NO_RESOURCE No group slot remains.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support multicast.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int
rzi_lorawan_add_multicast_session(const struct rzi_lorawan_multicast_session *session);

/**
 * @brief Stop and remove the multicast session with dev_addr.
 *
 * @param dev_addr Multicast device address in host byte order.
 *
 * @retval 0 Session removed.
 * @retval -RZI_ERR_NOT_FOUND No session uses this address.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support multicast.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_remove_multicast_session(uint32_t dev_addr);

/**
 * @brief Return the number of configured multicast sessions.
 *
 * @param[out] count Stored session count.
 *
 * @retval 0 Count stored.
 * @retval -RZI_ERR_INVALID count is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support multicast.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_multicast_count(size_t *count);

/**
 * @brief Copy one multicast session by index.
 *
 * Session keys are not returned. Key fields are zeroed.
 *
 * @param index Session index in the range 0 through count-1.
 * @param[out] session Stored session without keys.
 *
 * @retval 0 Session stored.
 * @retval -RZI_ERR_INVALID session is NULL or index is out of range.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support multicast.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_multicast_session(size_t index,
						   struct rzi_lorawan_multicast_session *session);

/**
 * @brief Stop and remove every multicast session.
 *
 * @retval 0 Sessions cleared.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support multicast.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_clear_multicast_sessions(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_LORAWAN_MULTICAST_H */
