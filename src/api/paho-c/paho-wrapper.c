/**
 Copyright 2016 Otavio Rodolfo Piske

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */
#include "paho-wrapper.h"
#include "paho-context.h"
#include "vmsl.h"

static inline paho_ctxt_t *paho_ctxt_cast(msg_ctxt_t *ctxt) {
	return (paho_ctxt_t *) ctxt->api_context;
}

msg_ctxt_t *paho_init(
	stat_io_t *stat_io, msg_opt_t opt, void *data, gru_status_t *status) {
	logger_t logger = gru_logger_get();

	logger(DEBUG, "Initializing Paho wrapper");

	msg_ctxt_t *msg_ctxt = msg_ctxt_init(stat_io, status);
	if (!msg_ctxt) {
		logger(FATAL, "Unable to initialize the messaging context");

		exit(1);
	}

	paho_ctxt_t *paho_ctxt = paho_context_init();

	if (!paho_ctxt) {
		logger(FATAL, "Unable to initialize the paho context");

		exit(1);
	}

	paho_ctxt->uri = gru_uri_clone(opt.uri, status);

	if (!gru_uri_set_scheme(&paho_ctxt->uri, "tcp")) {
		logger(FATAL, "Unable to adjust the connection URI");

		exit(1);
	}

	const char *connect_url =
		gru_uri_format(&paho_ctxt->uri, GRU_URI_FORMAT_NONE, status);

	logger(DEBUG, "Creating a client to %s with path %s ", connect_url, 
            paho_ctxt->uri.path);
	int rc = 0;
	if (opt.direction == MSG_DIRECTION_SENDER) {
		rc = MQTTClient_create(&paho_ctxt->client,
			connect_url,
			"msg-perf-tool-sender",
			MQTTCLIENT_PERSISTENCE_NONE,
			NULL);
	} else {
		rc = MQTTClient_create(&paho_ctxt->client,
			connect_url,
			"msg-perf-tool-receiver",
			MQTTCLIENT_PERSISTENCE_NONE,
			NULL);
	}

	if (rc != MQTTCLIENT_SUCCESS) {
		logger(FATAL, "Unable to create MQTT client handle: %d", rc);

		exit(-1);
	}

	logger(DEBUG, "Setting connection options");
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;

	logger(DEBUG, "Connecting to %s", connect_url);
	rc = MQTTClient_connect(paho_ctxt->client, &conn_opts);
	if (rc != MQTTCLIENT_SUCCESS) {
		logger(FATAL, "Unable to connect: %d", rc);

		exit(-1);
	}

	msg_ctxt->api_context = paho_ctxt;

	return msg_ctxt;
}

void paho_stop(msg_ctxt_t *ctxt, gru_status_t *status) {
	paho_ctxt_t *paho_ctxt = paho_ctxt_cast(ctxt);

	int rc = MQTTClient_disconnect(paho_ctxt->client, 10000);
	switch (rc) {
		case MQTTCLIENT_SUCCESS:
			break;
		default: {
			logger_t logger = gru_logger_get();

			logger(WARNING, "Unable to disconnect from the server: %d", rc);
		}
	}

	return;
}

void paho_destroy(msg_ctxt_t *ctxt, gru_status_t *status) {
	paho_ctxt_t *paho_ctxt = paho_ctxt_cast(ctxt);

	MQTTClient_destroy(&paho_ctxt->client);
}

struct paho_perf_pl {
	char *data;
	int size;
};

static struct paho_perf_pl paho_serialize_latency_info(msg_content_data_t *msg_content) {
	struct paho_perf_pl ret = {0};
	gru_timestamp_t ts = gru_time_now();

	char *formatted_ts = gru_time_write_str(&ts);

	asprintf(&ret.data, "%18s%s", formatted_ts, (char *) msg_content->data);
	ret.size = 18 + msg_content->size;
	gru_dealloc_string(&formatted_ts);
	return ret;
}

static void paho_serialize_clean(struct paho_perf_pl *pl) {
	free(pl->data);
	pl->size = 0;
}

vmsl_stat_t paho_send(msg_ctxt_t *ctxt, msg_content_data_t *data, gru_status_t *status) {
	MQTTClient_deliveryToken token;
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	
	paho_ctxt_t *paho_ctxt = paho_ctxt_cast(ctxt);

	// QoS0, At most once:
	pubmsg.qos = 0;
	pubmsg.retained = false;
	int rc = 0;

	if (ctxt->msg_opts.statistics & MSG_STAT_LATENCY) { 
		struct paho_perf_pl pl = paho_serialize_latency_info(data);

		pubmsg.payload = pl.data;
		pubmsg.payloadlen = pl.size;

		// mpt_trace("Sending message '%s' to %s", pl.data, paho_ctxt->uri.path);

	logger_t logger = gru_logger_get();
		logger(DEBUG, "Sending message with latency '%s' to %s", data->data, paho_ctxt->uri.path);

		rc = MQTTClient_publishMessage(
			paho_ctxt->client, ctxt->msg_opts.uri.path, &pubmsg, &token);
		paho_serialize_clean(&pl);
	}
	else {
		// mpt_trace("Sending message '%s' to %s", data->data, paho_ctxt->uri.path);
		logger_t logger = gru_logger_get();

		logger(DEBUG, "Sending message '%s' to %s", data->data, paho_ctxt->uri.path);

		pubmsg.payload = data->data;
		pubmsg.payloadlen = data->size;

		rc = MQTTClient_publishMessage(
			paho_ctxt->client, ctxt->msg_opts.uri.path, &pubmsg, &token);
	}

	switch (rc) {
		case MQTTCLIENT_SUCCESS:
			break;
		default: {
			gru_status_set(
				status, GRU_FAILURE, "Unable to publish the message: error %d", rc);

			return VMSL_ERROR;
		}
	}

	rc = MQTTClient_waitForCompletion(paho_ctxt->client, token, TIMEOUT);
	switch (rc) {
		case MQTTCLIENT_SUCCESS:
			break;
		default: {
			gru_status_set(status, GRU_FAILURE, "Unable to synchronize: error %d", rc);

			return VMSL_ERROR;
		}
	}

	mpt_trace("Delivered message %d", token);
	return VMSL_SUCCESS;
}


vmsl_stat_t paho_subscribe(msg_ctxt_t *ctxt, void *data, gru_status_t *status) {
	paho_ctxt_t *paho_ctxt = paho_ctxt_cast(ctxt);

	logger_t logger = gru_logger_get();

	logger(DEBUG, "Subscribing to %s", paho_ctxt->uri.path);

	int rc = MQTTClient_subscribe(
		paho_ctxt->client, paho_ctxt->uri.path, QOS_AT_MOST_ONCE);

	switch (rc) {
		case MQTTCLIENT_SUCCESS:
			break;
		default: {
			gru_status_set(status, GRU_FAILURE, "Unable to subscribe: error %d", rc);

			return VMSL_ERROR;
		}
	}

	logger(DEBUG, "Subscribed to the topic");
	return VMSL_SUCCESS;
}

vmsl_stat_t paho_receive(
	msg_ctxt_t *ctxt, msg_content_data_t *content, gru_status_t *status) {
	MQTTClient_message *msg = NULL;
	paho_ctxt_t *paho_ctxt = paho_ctxt_cast(ctxt);
	unsigned long timeout = 10000L;

	int tlen = 0;
	char *topic_name;
	int rc = MQTTClient_receive(paho_ctxt->client, &topic_name, &tlen, &msg, timeout);

	switch (rc) {
		case MQTTCLIENT_SUCCESS: {
			if (!msg) {
				return VMSL_SUCCESS | VMSL_NO_DATA;
			}

			break;
		}
		case MQTTCLIENT_TOPICNAME_TRUNCATED: {
			logger_t logger = gru_logger_get();

			logger(WARNING, "Topic name truncated");
			break;
		}
		default: {
			gru_status_set(status, GRU_FAILURE, "Unable to receive data: error %d", rc);
			content->errors++;
			return VMSL_ERROR;
		}
	}

	if (ctxt->msg_opts.statistics & MSG_STAT_LATENCY) { 
		gru_timestamp_t now = gru_time_now();

		char header[18] = {0};
		sscanf(msg->payload, "%17s", &header);

		gru_timestamp_t created = gru_time_read_str(header);
		statistics_latency(ctxt->stat_io, created, now);
	}

	if (msg->payloadlen > content->capacity) {
		memcpy(content->data, msg->payload, content->capacity - 1);
	}
	else {
		memcpy(content->data, msg->payload, msg->payloadlen);
	}
	
	content->count++;

	return VMSL_SUCCESS;
}

bool paho_vmsl_assign(vmsl_t *vmsl) {
	logger_t logger = gru_logger_get();

	logger(INFO, "Initializing MQTT protocol");

	vmsl->init = paho_init;
	vmsl->receive = paho_receive;
	vmsl->subscribe = paho_subscribe;
	vmsl->send = paho_send;
	vmsl->stop = paho_stop;
	vmsl->destroy = paho_destroy;

	return true;
}