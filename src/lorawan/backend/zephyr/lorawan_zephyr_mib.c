/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Read OTAA session identity from the loramac-node MIB.
 *
 * Compiled into the loramac-node library so LoRaMac headers stay private
 * to that library.
 */

#include <errno.h>
#include <string.h>

#include <LoRaMac.h>

int rzi_lorawan_zephyr_mib_dev_addr(uint32_t *dev_addr)
{
	MibRequestConfirm_t request;

	if (dev_addr == NULL) {
		return -EINVAL;
	}
	request.Type = MIB_DEV_ADDR;
	if (LoRaMacMibGetRequestConfirm(&request) != LORAMAC_STATUS_OK) {
		return -EIO;
	}
	*dev_addr = request.Param.DevAddr;
	return 0;
}

static int copy_session_key(KeyIdentifier_t key_id, uint8_t key[16])
{
#ifdef SOFT_SE
	MibRequestConfirm_t request;
	uint8_t i;

	if (key == NULL) {
		return -EINVAL;
	}
	request.Type = MIB_NVM_CTXS;
	if (LoRaMacMibGetRequestConfirm(&request) != LORAMAC_STATUS_OK ||
	    request.Param.Contexts == NULL) {
		return -EIO;
	}
	for (i = 0; i < NUM_OF_KEYS; i++) {
		const Key_t *item = &request.Param.Contexts->SecureElement.KeyList[i];

		if (item->KeyID == key_id) {
			memcpy(key, item->KeyValue, 16);
			return 0;
		}
	}
	return -ENOENT;
#else
	(void)key_id;
	(void)key;
	return -ENOTSUP;
#endif
}

int rzi_lorawan_zephyr_mib_nwk_skey(uint8_t key[16])
{
	return copy_session_key(NWK_S_ENC_KEY, key);
}

int rzi_lorawan_zephyr_mib_app_skey(uint8_t key[16])
{
	return copy_session_key(APP_S_KEY, key);
}
