/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal backend contract for raw LoRa and FSK.
 */
#ifndef RZI_LORA_BACKEND_H
#define RZI_LORA_BACKEND_H

#include <rzi/lora/lora.h>

/** One compiled backend implements this table. */
struct rzi_lora_backend_api {
	/** Enter LoRa or FSK operation. */
	int (*start)(enum rzi_lora_modulation modulation);
	/** Leave the radio. NULL is treated as success. */
	int (*stop)(void);
	/** Push the stored configuration to the radio. */
	int (*apply_config)(const struct rzi_lora_config *config);
	/** Transmit one payload. */
	int (*send)(const uint8_t *data, size_t size);
	/** Enter or leave receive. */
	int (*receive)(uint32_t timeout_ms);
	/** Start an RSSI test. */
	int (*test_rssi)(void);
	/** Start a CW tone test. */
	int (*test_tone)(void);
	/** Transmit PER packets. */
	int (*test_tx)(uint32_t packet_count);
	/** Receive PER packets. */
	int (*test_rx)(uint32_t packet_count);
	/** Start a continuous wave. */
	int (*test_cw)(uint32_t frequency_hz, int8_t power_dbm, uint32_t duration_ms);
	/** Stop an ongoing radio test. */
	int (*test_stop)(void);
	/** Poll for deferred receive results. */
	void (*poll)(void);
};

/** Selected backend, supplied by one backend translation unit. */
extern const struct rzi_lora_backend_api rzi_lora_backend;

/** Publish a completed P2P uplink to registered callbacks. */
void rzi_lora_publish_tx_done(int error);

/** Publish a received P2P payload to registered callbacks. */
void rzi_lora_publish_rx(const uint8_t *data, size_t size, int16_t rssi_dbm, int8_t snr_quarter_db);

#endif /* RZI_LORA_BACKEND_H */
