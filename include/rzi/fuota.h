/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI LoRaWAN FUOTA coordination API.
 */
#ifndef RZI_FUOTA_H
#define RZI_FUOTA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_fuota RZI FUOTA service
 *  @brief Backend-independent firmware-update coordination for ChirpStack.
 *
 *  LoRaWAN Clock Synchronization, Remote Multicast Setup and Fragmentation
 *  remain inside the selected backend. This service owns session state,
 *  image access, optional MCUboot installation and reboot.
 *
 *  @since 0.2
 *  @version 0.2.0
 *  @{
 */

/** Observable FUOTA session states. */
enum rzi_fuota_state {
	/** Service has not started or the device is not joined. */
	RZI_FUOTA_STATE_IDLE,
	/** Joined and waiting for a ChirpStack or network-server session. */
	RZI_FUOTA_STATE_READY,
	/** A multicast fragment session is active. */
	RZI_FUOTA_STATE_TRANSFERRING,
	/** The backend reconstructed a complete firmware image. */
	RZI_FUOTA_STATE_COMPLETE,
	/** The latest session failed to reconstruct an image. */
	RZI_FUOTA_STATE_FAILED,
	/** Image installation or reboot is in progress. */
	RZI_FUOTA_STATE_APPLYING,
};

/** Snapshot returned by @ref rzi_fuota_get_status. */
struct rzi_fuota_status {
	/** Current coordination state. */
	enum rzi_fuota_state state;
	/** True when a reconstructed image can be read or applied. */
	bool image_ready;
	/** Detected or configured image size, or zero when unknown. */
	size_t image_size;
	/** Zero, or the negative errno from the latest failure. */
	int last_error;
};

/**
 * @brief Asynchronous FUOTA callbacks.
 *
 * Callbacks execute in the LoRaWAN dispatcher thread. They must return
 * promptly and must not call @ref rzi_fuota_apply.
 *
 * @since 0.2
 */
struct rzi_fuota_callbacks {
	/** Called after every observable state transition. */
	void (*state_changed)(enum rzi_fuota_state state, void *user_data);
	/** Called when a multicast fragment session starts. */
	void (*session_started)(void *user_data);
	/** Called with zero after a successful transfer, otherwise errno. */
	void (*complete)(int status, void *user_data);
	/** Opaque pointer passed to every callback. */
	void *user_data;
};

/**
 * @brief Register optional FUOTA callbacks.
 *
 * The table is copied. Register before @ref rzi_fuota_start to observe the
 * first READY transition.
 *
 * @param callbacks Callback table copied before this function returns.
 *
 * @retval 0 Callbacks stored.
 * @retval -EINVAL callbacks is NULL or empty.
 * @retval -EWOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_fuota_register_callbacks(const struct rzi_fuota_callbacks *callbacks);

/**
 * @brief Start FUOTA coordination.
 *
 * Clock synchronization starts after the next successful LoRaWAN join. The
 * backend then accepts ChirpStack unicast setup and multicast fragments.
 *
 * @retval 0 Coordination started or already running.
 * @retval -ENOTSUP The selected backend does not advertise FUOTA.
 * @retval -EWOULDBLOCK Called from an ISR.
 *
 * @note Thread context only. rzi_lorawan_start() starts this service when
 *       CONFIG_RZI_LORAWAN_FUOTA is enabled.
 * @since 0.2
 */
__must_check int rzi_fuota_start(void);

/**
 * @brief Return the current FUOTA session snapshot.
 *
 * @param[out] status Caller-owned status object.
 *
 * @retval 0 Status stored.
 * @retval -EINVAL status is NULL.
 * @retval -EWOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_fuota_get_status(struct rzi_fuota_status *status);

/**
 * @brief Optionally override the reconstructed image size in bytes.
 *
 * The service prefers the size reported by the backend, then an MCUboot
 * image header. Call this only when both are unavailable, for example a
 * raw binary on a backend that does not export the fragment session size.
 *
 * @param size Image size in bytes.
 *
 * @retval 0 Size stored.
 * @retval -EINVAL size is zero.
 * @retval -EWOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_fuota_set_expected_size(size_t size);

/**
 * @brief Read bytes from the reconstructed firmware image.
 *
 * @param offset Byte offset from the start of the image.
 * @param buffer Destination buffer.
 * @param size Number of bytes to copy.
 *
 * @retval 0 Bytes copied.
 * @retval -EINVAL buffer is NULL or size is zero.
 * @retval -ENOENT No reconstructed image is available.
 * @retval -ENOTSUP The backend cannot expose the stored image.
 * @retval -EWOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_fuota_read_image(size_t offset, void *buffer, size_t size);

/**
 * @brief Install the reconstructed image and reboot.
 *
 * When MCUboot image management is enabled the image is copied into the
 * secondary slot and a test upgrade is requested. The function does not
 * return on a successful reboot.
 *
 * @retval -ENOENT No reconstructed image is available.
 * @retval -ENODATA The image size is unknown.
 * @retval -ENOTSUP No installer is compiled into this firmware.
 * @retval -EWOULDBLOCK Called from an ISR.
 *
 * @warning Must not be called from a FUOTA callback.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_fuota_apply(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_FUOTA_H */
