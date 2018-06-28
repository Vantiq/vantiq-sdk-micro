#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "CUnit/Basic.h"
#include "vme.h"
#include "cjson.h"

#define MAXLINELENGTH   2048

void trim_trailing_wspace(char *buffer)
{
    size_t len = strlen(buffer)-1;
    while (isspace(buffer[len])) {
        len--;
    }
    buffer[len+1] = 0;
}

void test_inserts() {
    char buffer[MAXLINELENGTH];

    vmeconfig_t config;
    if (vme_parse_config("config.properties", &config) == -1)
        CU_ASSERT_EQUAL_FATAL(-1, 3);

    VME vme = vme_init(config.vantiq_url, config.vantiq_token, 1);
    FILE *jsonFile = fopen("dataset.json", "r");
    char *rsURI = vme_build_custom_rsuri(vme, "VME_Test", NULL);

    vmebuf_t *msg = buf_alloc();
    vmebuf_push(msg, '[');

    int count = 1;
    while (fgets(buffer, sizeof(buffer), jsonFile) != NULL) {
        trim_trailing_wspace(buffer);
        if (msg->len > 1) {
            vmebuf_push(msg, ',');
        } else {
            //printf("count is %d in first instance of batch %s\n", count, buffer);
        }
        vmebuf_concat(msg, buffer, strlen(buffer));

        if ((count % 500) == 0) {
            vmebuf_push(msg, ']');
            vme_result_t *result = vme_insert(vme, rsURI, msg->data, msg->len);
            if (result->vme_error_msg != NULL) {
                cJSON *json = cJSON_Parse((const char *)msg->data);
                char *pretty = cJSON_Print(json);
                printf("===error %s after reading %d inserting %s\n", result->vme_error_msg, count, pretty);
                fflush(stdout);
                free(pretty);
                cJSON_Delete(json);
                fprintf(stderr, "insert resulted in err: %s", result->vme_error_msg);
                fflush(stderr);
            }
            CU_ASSERT_PTR_NULL(result->vme_error_msg);
            vme_free_result(result);
            vmebuf_truncate(msg);
            vmebuf_push(msg, '[');
        }
        count++;
    }
    if (msg->len > 1) {
        vme_result_t *result = vme_insert(vme, rsURI, msg->data, msg->len);
        if (result->vme_error_msg != NULL) {
            fprintf(stderr, "insert resulted in err: %s", result->vme_error_msg);
        }
        CU_ASSERT_PTR_NULL(result->vme_error_msg);

        vme_free_result(result);
    }
    /* insert a badly formatted instance -- should error */
    const char *instance = "{\"amount\" : 711.69, \"description\" : \"happy bday\", \"firstName\" : \"Jeffrey\", \"ID\" : \"deadbeef-cafe-4645-844d-e70c7fc76961\", \"lastName\" : \"Meredith\", \"timestamp\" : \"2018-06-20T05:49:23.208Z\"}";
    vme_result_t *result = vme_insert(vme, rsURI, instance, strlen(instance));
    //fprintf(stderr, "inserted an badly formatted instance to VME_Test: %.*s\n", (int)result->vme_size, result->vme_error_msg);
    CU_ASSERT_PTR_NOT_NULL(result->vme_error_msg);
    vme_free_result(result);

    free(rsURI);
    vmebuf_dealloc(msg);
    fclose(jsonFile);
    vme_teardown(vme);
    CU_PASS("test inserts");
}
