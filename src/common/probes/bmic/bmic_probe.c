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
#include "bmic_probe.h"

#define as_mb(x) (x / (1024 * 1024))

static FILE *report;
static const char *name = "bmic";
static bmic_context_t ctxt = {0};

static void print_queue_stat(bmic_queue_stat_t stat) {
	fprintf(report, "%" PRId64 ";%" PRId64 ";%" PRId64 ";%" PRId64 "\n",
		stat.queue_size, stat.consumer_count, stat.msg_ack_count, stat.msg_exp_count);
}

static void print_mem(bmic_java_mem_info_t *mem) {
	fprintf(report, "%" PRId64 ";%" PRId64 ";%" PRId64 ";%" PRId64 ";",
		as_mb(mem->init), as_mb(mem->committed), as_mb(mem->max), as_mb(mem->used));
}

probe_entry_t *bmic_entry(gru_status_t *status) {
	probe_entry_t *ret = gru_alloc(sizeof(probe_entry_t), status);
	gru_alloc_check(ret, NULL);

	ret->init = bmic_init;
	ret->collect = bmic_collect;
	ret->stop = bmic_stop;
	ret->name = bmic_name;

	ret->cancel = false;

	return ret;
	
}

bool bmic_init(const options_t *options, gru_status_t *status) {
	logger_t logger = gru_logger_get();

	logger(DEBUG, "Creating report file");

	char filename[64] = {0};

	snprintf(filename, sizeof(filename) - 1, "broker-jvm-statistics-%d.csv", getpid());

	report = gru_io_open_file(options->logdir, filename, status);

	if (!report) {
		return false;
	}

	bool ret_ctxt = mpt_init_bmic_ctxt(options, &ctxt, status);
	if (!ret_ctxt) {
		return false;
	}

	fprintf(report, "timestamp;load;open fds;free fds;free mem;swap free;swap committed;");
	fprintf(report, "eden inital;eden committed;eden max;eden used;");
	fprintf(report, "survivor inital;survivor committed;survivor max;survivor used;");
	fprintf(report, "tenured inital;tenured committed;tenured max;tenured used;");
	fprintf(report, "pm inital;pm committed;pm max;pm used;");
	fprintf(report, "queue size;consumers;ack;exp;\n");

	return true;
}


int bmic_collect(gru_status_t *status) {
	const options_t *options = get_options_object();

	bmic_api_interface_t *api = ctxt.api;
	bmic_java_info_t jinfo = api->java.java_info(ctxt.handle, status);
	while (true) { 
		bmic_queue_stat_t qstats = {0};
		mpt_get_queue_stats(&ctxt, &options->uri.path[1], &qstats, status);
		
		if (gru_status_error(status)) {
			return 1;
		}

		bmic_java_os_info_t osinfo = api->java.os_info(ctxt.handle, status);
		bmic_java_mem_info_t eden = api->java.eden_info(ctxt.handle, status);
		bmic_java_mem_info_t survivor = api->java.survivor_info(ctxt.handle, status);
		bmic_java_mem_info_t tenured = api->java.tenured_info(ctxt.handle, status);
		bmic_java_mem_info_t metaspace;
		bmic_java_mem_info_t permgen;

		if (jinfo.memory_model == BMIC_JAVA_MODERN) {
			metaspace = api->java.metaspace_info(ctxt.handle, status);
		} else {
			permgen = api->java.permgen_info(ctxt.handle, status);
		}

		char tm_creation_buff[64] = {0};
		gru_timestamp_t now = gru_time_now();

		struct tm result;
		struct tm *creation_tm = localtime_r(&now.tv_sec, &result);

		if (!creation_tm) {
			logger_t logger = gru_logger_get();
			
			logger(ERROR, "Unable to calculate current localtime");

			return 1;
		}

		strftime(tm_creation_buff, sizeof(tm_creation_buff), "%Y-%m-%d %H:%M:%S", 
			creation_tm);

		fprintf(report,"%s;%.1f;", tm_creation_buff, osinfo.load_average);
		fprintf(report, "%" PRId64";%" PRId64 ";", 	osinfo.open_fd, 
			(osinfo.max_fd - osinfo.open_fd));
		fprintf(report, "%" PRId64 ";", as_mb(osinfo.mem_free));
		fprintf(report, "%" PRId64 ";%" PRId64";", as_mb(osinfo.swap_free),
			as_mb(osinfo.swap_committed));

		print_mem(&eden);
		print_mem(&survivor);
		print_mem(&tenured);

		if (jinfo.memory_model == BMIC_JAVA_MODERN) {
			print_mem(&metaspace);
		} else {
			print_mem(&permgen);
		}

		print_queue_stat(qstats);

		fflush(report);
		sleep(10);
	}


	return 0;
}

void bmic_stop() {
	fclose(report);
}

const char *bmic_name() {
	return name;
}