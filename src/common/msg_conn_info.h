/**
 *    Copyright 2017 Otavio Rodolfo Piske
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
#ifndef MSG_CONN_INFO_H
#define MSG_CONN_INFO_H

#include <stdlib.h>
#include <stdbool.h>

#include <uuid/uuid.h>

#include <log/gru_logger.h>

#include "msg_direction.h"

/**
 * Default, non-unique sender id
 */
#define MSG_CONN_ID_DEFAULT_SENDER_ID "msg-perf-tool-sender"

/**
 * Default, non-unique receiver id
 */
#define MSG_CONN_ID_DEFAULT_RECEIVER_ID "msg-perf-tool-receiver"

typedef struct msg_conn_info_t_ {
	char *id;
} msg_conn_info_t;

void msg_conn_info_gen_id(msg_conn_info_t *conn_info);

#endif /* MSG_CONN_INFO_H */