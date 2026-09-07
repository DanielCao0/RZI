/* SPDX-License-Identifier: Apache-2.0 */
/** @file @brief Transport-independent RUI3-compatible AT service. */
#ifndef RZI_AT_H
#define RZI_AT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum rzi_at_operation {
	RZI_AT_OP_RUN,
	RZI_AT_OP_HELP,
	RZI_AT_OP_READ,
	RZI_AT_OP_WRITE,
};

#define RZI_AT_ALLOW_RUN   (1U << RZI_AT_OP_RUN)
#define RZI_AT_ALLOW_READ  (1U << RZI_AT_OP_READ)
#define RZI_AT_ALLOW_WRITE (1U << RZI_AT_OP_WRITE)

enum rzi_at_status {
	RZI_AT_STATUS_OK,
	RZI_AT_STATUS_ERROR,
	RZI_AT_STATUS_PARAM_ERROR,
	RZI_AT_STATUS_BUSY_ERROR,
	RZI_AT_STATUS_NO_NETWORK_JOINED,
	RZI_AT_STATUS_TEST_PARAM_OVERFLOW,
};

struct rzi_at_request {
	enum rzi_at_operation operation;
	const char *argument;
};

typedef int (*rzi_at_command_handler_t)(const struct rzi_at_request *request, void *user_data);

/** Command names exclude the leading "AT+" and use uppercase ASCII. */
struct rzi_at_command {
	const char *name;
	const char *help;
	uint8_t allowed_operations;
	rzi_at_command_handler_t handler;
	void *user_data;
};

/** Synchronous byte transport used for replies and unsolicited events. */
struct rzi_at_transport {
	int (*write)(const uint8_t *data, size_t size, void *user_data);
	void *user_data;
};

/** Register commands before starting the process-wide AT service. */
int rzi_at_register(const struct rzi_at_command *commands, size_t count);

/** Start the AT service on a transport. */
int rzi_at_start(const struct rzi_at_transport *transport);

/** Supply received bytes. This function is ISR-safe. */
int rzi_at_receive(const uint8_t *data, size_t size);

int rzi_at_respond_status(enum rzi_at_status status);
int rzi_at_respond_value(const char *format, ...);
int rzi_at_publish_event(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
