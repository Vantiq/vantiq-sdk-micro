#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "vme.h"
#include "dpi_client.h"
#include "cjson.h"

static char *usage = "vipo <config file path>";

vmeconfig_t config;

#ifdef CU_TEST
int vipo_main(int argc, char *argv[])
{
#else
int main(int argc, char *argv[])
{
#endif
	if (argc != 2) {
		fprintf(stderr, "incorrect number of arguments. %s", usage);
        exit(1);
	}

    vmeconfig_t config;
    vme_parse_config(argv[1], &config);
	VME vme = vme_init(config.vantiq_url, config.vantiq_token, 1);

    dpi_client_t *dpic = NULL;
    int retries = 0;
    do {
        dpic =
            (config.dpi_port != NULL ?
                init_dpi_inet_client(config.dpi_port) :
                init_dpi_usock_client(config.dpi_socket_path));
        if (dpic == NULL) {
            if (config.dpi_port != NULL)
                fprintf(stderr, "failed to establish connection to the DPI on localhost port %s", config.dpi_port);
            else
                fprintf(stderr, "failed to establish connection to the DPI on unix socket %s", config.dpi_socket_path);
        }
    } while (retries-- > 0);

    char *msg = NULL;
    vme_result_t *result = NULL;
    if (dpic != NULL) {
        /* this will block until message receipt */
        msg = fetch_discovery_msg(dpic);
        // TODO: should only have to specify the topic not the full REST API path
        result = vme_publish(vme, "/ChinaUnicom/SmartHome/Discovery", msg, strlen(msg));
        if (result->vme_error_msg != NULL) {
            fprintf(stderr, "publish to topic %s failed with: %s\n", "/ChinaUnicom/SmartHome/Discovery", result->vme_error_msg);
        }
        free(msg);
        vme_free_result(result);
    }

    vme_teardown(vme);
    exit(0);
}
