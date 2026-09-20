/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI raw LoRa and FSK service API.
 */
#ifndef RZI_LORA_LORA_H
#define RZI_LORA_LORA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/toolchain.h>

#include <rzi/err.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_lora RZI raw LoRa / FSK
 *  @brief Thread-safe C API for P2P LoRa and FSK.
 *  @since 0.3
 *  @version 0.4.0
 *  @{
 */

/** Maximum storage reserved for a P2P payload. */
#define RZI_LORA_MAX_PAYLOAD 255

/** Physical modulation selected by rzi_lora_start(). */
enum rzi_lora_modulation {
	/** LoRa chirp modulation. */
	RZI_LORA_MOD_LORA = 0,
	/** FSK modulation. */
	RZI_LORA_MOD_FSK,
};

/** Radio configuration used by P2P send, receive, and radio tests. */
struct rzi_lora_config {
	/** Carrier frequency in hertz. */
	uint32_t frequency_hz;
	/** LoRa spreading factor, 5 through 12. */
	uint8_t spreading_factor;
	/** LoRa bandwidth index 0 through 9, or FSK bandwidth in hertz. */
	uint32_t bandwidth;
	/** LoRa coding-rate index 0 through 3. */
	uint8_t coding_rate;
	/** Preamble length in symbols. */
	uint16_t preamble_length;
	/** Transmit power in dBm, 5 through 22. */
	int8_t tx_power_dbm;
	/** LoRa sync word. */
	uint16_t sync_word;
	/** Invert IQ when true. */
	bool iq_inverted;
	/** XOR the payload with key and iv when true. */
	bool encrypt;
	/** Payload key used when encrypt is true. */
	uint8_t key[16];
	/** Payload IV used when encrypt is true. */
	uint8_t iv[16];
	/** Request channel activity detection before TX when true. */
	bool cad;
	/** Use a fixed payload length when true. */
	bool fixed_length;
	/** FSK bitrate in bits per second. */
	uint32_t fsk_bitrate;
	/** FSK frequency deviation in hertz. */
	uint32_t fsk_deviation;
	/** Symbol timeout for LoRa receive. */
	uint8_t symbol_timeout;
};

/** Asynchronous P2P results. The table is copied by register_callbacks(). */
struct rzi_lora_callbacks {
	/**
	 * @brief Called after an accepted P2P uplink finishes.
	 *
	 * @param error Zero on success, otherwise a negative RZI_ERR_* value.
	 * @param user_data Pointer from this table.
	 */
	void (*tx_done)(int error, void *user_data);
	/**
	 * @brief Called with one received P2P payload.
	 *
	 * The payload pointer is valid only for the duration of the callback.
	 *
	 * @param data Decrypted payload bytes.
	 * @param size Number of bytes in data.
	 * @param rssi_dbm RSSI in dBm.
	 * @param snr_quarter_db SNR in quarter-dB units.
	 * @param user_data Pointer from this table.
	 */
	void (*rx_done)(const uint8_t *data, size_t size, int16_t rssi_dbm, int8_t snr_quarter_db,
			void *user_data);
	/** Opaque pointer passed to every callback. */
	void *user_data;
};

/**
 * @brief Register P2P callbacks.
 *
 * The table is copied before this function returns. A NULL function pointer
 * disables that notification.
 *
 * @param callbacks Callback table.
 *
 * @retval 0 Table copied.
 * @retval -RZI_ERR_INVALID callbacks is NULL.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_register_callbacks(const struct rzi_lora_callbacks *callbacks);

/**
 * @brief Start the raw LoRa service in LoRa or FSK mode.
 *
 * @param modulation Physical modulation used by later send and receive calls.
 *
 * @retval 0 Service started.
 * @retval -RZI_ERR_INVALID modulation is not a supported enumerator.
 * @retval -RZI_ERR_NOT_SUPPORTED The selected backend cannot start.
 * @retval -RZI_ERR_BUSY The radio is owned by another service.
 * @retval -RZI_ERR_NOT_READY The backend is not initialized.
 * @retval -RZI_ERR_NO_DEVICE The radio device is missing.
 * @retval -RZI_ERR_IO The radio failed to start.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_start(enum rzi_lora_modulation modulation);

/**
 * @brief Stop the raw LoRa service and any radio test.
 *
 * @retval 0 Service stopped.
 * @retval -RZI_ERR_NOT_SUPPORTED The selected backend cannot stop.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_stop(void);

/**
 * @brief Return whether the raw LoRa service has started.
 *
 * @return True after a successful rzi_lora_start().
 *
 * @note May be called from any thread.
 * @since 0.3
 */
bool rzi_lora_is_started(void);

/**
 * @brief Copy the current P2P configuration.
 *
 * @param config Destination filled before this function returns.
 *
 * @retval 0 Configuration copied.
 * @retval -RZI_ERR_INVALID config is NULL.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_get_config(struct rzi_lora_config *config);

/**
 * @brief Replace the current P2P configuration.
 *
 * The configuration is copied before this function returns. When the service
 * has started, the backend is asked to apply it immediately.
 *
 * @param config Configuration to store.
 *
 * @retval 0 Configuration stored.
 * @retval -RZI_ERR_INVALID config is NULL or a field is out of range.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend rejected the configuration.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_set_config(const struct rzi_lora_config *config);

/**
 * @brief Send one P2P payload.
 *
 * A return value of zero means the request was accepted. Completion is
 * reported through tx_done(). The payload is copied before this function
 * returns.
 *
 * @param data Payload bytes.
 * @param size Number of bytes in data, 1 through RZI_LORA_MAX_PAYLOAD.
 *
 * @retval 0 Request accepted.
 * @retval -RZI_ERR_INVALID data is NULL or size is out of range.
 * @retval -RZI_ERR_NOT_SUPPORTED The service has not started or the backend cannot send.
 * @retval -RZI_ERR_BUSY The radio is busy.
 * @retval -RZI_ERR_NOT_READY The backend is not initialized.
 * @retval -RZI_ERR_NO_DEVICE The radio device is missing.
 * @retval -RZI_ERR_IO The radio operation failed.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_send(const uint8_t *data, size_t size);

/**
 * @brief Enter or leave P2P receive.
 *
 * @param timeout_ms 0 stops receive, 65535 receives continuously, other
 *        values receive until that timeout.
 *
 * @retval 0 Request accepted.
 * @retval -RZI_ERR_NOT_SUPPORTED The service has not started or the backend cannot receive.
 * @retval -RZI_ERR_BUSY The radio is busy.
 * @retval -RZI_ERR_NOT_READY The backend is not initialized.
 * @retval -RZI_ERR_NO_DEVICE The radio device is missing.
 * @retval -RZI_ERR_IO The radio operation failed.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_receive(uint32_t timeout_ms);

/**
 * @brief Return whether P2P receive is active.
 *
 * @return True after a successful rzi_lora_receive() with a non-zero timeout.
 *
 * @note May be called from any thread.
 * @since 0.3
 */
bool rzi_lora_receive_active(void);

/**
 * @brief Poll the backend for deferred receive and test results.
 *
 * AT and other clients may call this from a thread. Backends that complete
 * operations synchronously may treat this as a no-op.
 *
 * @note Thread context only.
 * @since 0.3
 */
void rzi_lora_poll(void);

/**
 * @brief Start an RSSI measurement test.
 *
 * @retval 0 Request accepted.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not implement radio tests.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_test_rssi(void);

/**
 * @brief Start a continuous-wave tone test.
 *
 * @retval 0 Request accepted.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not implement radio tests.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_test_tone(void);

/**
 * @brief Transmit packet_count PER test packets.
 *
 * @param packet_count Number of packets to transmit.
 *
 * @retval 0 Request accepted.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not implement radio tests.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_test_tx(uint32_t packet_count);

/**
 * @brief Receive packet_count PER test packets.
 *
 * @param packet_count Number of packets to receive.
 *
 * @retval 0 Request accepted.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not implement radio tests.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_test_rx(uint32_t packet_count);

/**
 * @brief Start a continuous wave at frequency_hz.
 *
 * @param frequency_hz Carrier frequency in hertz.
 * @param power_dbm Transmit power in dBm.
 * @param duration_ms Requested duration; backends may ignore it.
 *
 * @retval 0 Request accepted.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not implement radio tests.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_test_cw(uint32_t frequency_hz, int8_t power_dbm, uint32_t duration_ms);

/**
 * @brief Stop an ongoing radio test.
 *
 * @retval 0 Request accepted.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not implement radio tests.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lora_test_stop(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_LORA_LORA_H */
