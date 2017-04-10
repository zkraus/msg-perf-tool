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
 #include "maestro_main.h"

static void show_help(char **argv) {
	gru_cli_program_usage("mpt-maestro", argv[0]);

	gru_cli_option_help("help", "h", "show this help");
    gru_cli_option_help("maestro-url", "m", "maestro URL to connect to");
}

static char *start = "001";


int main(int argc, char **argv) {
	int c;
	int option_index = 0;

	options_t *options = options_new();
	set_options_object(options);
	
	gru_status_t status = gru_status_new();

	if (!options) {
		return EXIT_FAILURE;
	}

	if (argc < 2) {
		show_help(argv);

		return EXIT_FAILURE;
	}

	gru_logger_set(gru_logger_default_printer);

	while (1) {

		static struct option long_options[] = {
			{"maestro-url", true, 0, 'm'},
			{"help", false, 0, 'h'},
			{0, 0, 0, 0}};

		c = getopt_long(argc, argv, "m:h", long_options, &option_index);
		if (c == -1) {
			break;
		}

		switch (c) {
			case 'm':
				options->maestro_uri = gru_uri_parse(optarg, &status);
				if (gru_status_error(&status)) {
					fprintf(stderr, "%s", status.message);
					goto err_exit;
				}
				break;
			case 'h':
				show_help(argv);
				return EXIT_SUCCESS;
			default:
				printf("Invalid or missing option\n");
				show_help(argv);
				return EXIT_SUCCESS;
		}
	}
	
	if (maestro_loop(&status) != 0) {
		goto err_exit;
	}

	options_destroy(&options);
	return EXIT_SUCCESS;

err_exit:
	options_destroy(&options);
	return EXIT_FAILURE;
}