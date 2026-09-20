/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI LoRaWAN channel RSSI scan API.
 */
#ifndef RZI_LORAWAN_CHANNEL_SCAN_H
#define RZI_LORAWAN_CHANNEL_SCAN_H

#include <stddef.h>
#include <stdint.h>
#include <zephyr/toolchain.h>

#include <rzi/err.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @ingroup rzi_lorawan
 *  @{
 */

/** One scanned-channel RSSI sample. */
struct rzi_lorawan_channel_rssi {
	/** Channel index. */
	uint32_t channel;
	/** Channel-mask bit that contained this channel. */
	uint16_t mask;
	/** Last RSSI observed on the channel in dBm. */
	int8_t rssi_dbm;
};

/**
 * @brief Return the number of stored channel RSSI samples.
 *
 * @param[out] count Stored sample count.
 *
 * @retval 0 Count stored.
 * @retval -RZI_ERR_INVALID count is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support channel scanning.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_channel_rssi_count(size_t *count);

/**
 * @brief Copy one channel RSSI sample by index.
 *
 * @param index Sample index in the range 0 through count-1.
 * @param[out] rssi Stored sample.
 *
 * @retval 0 Sample stored.
 * @retval -RZI_ERR_INVALID rssi is NULL or index is out of range.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support channel scanning.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_channel_rssi(size_t index, struct rzi_lorawan_channel_rssi *rssi);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_LORAWAN_CHANNEL_SCAN_H */
