#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "CUnit/Basic.h"
#include "vme.h"
#include "cjson.h"

#define MAXLINELENGTH   2048

size_t select_callback(void *state, const char *data, size_t size)
{
    // don't do anything with the data, just keep it coming.
    //printf("tail end of data buffer %.15s", data + (size-15));
    return size;
}

void test_selects()
{
    vmeconfig_t config;
    if (vme_parse_config("config.properties", &config) == -1)
        CU_ASSERT_EQUAL_FATAL(-1, 3);

    VME vme = vme_init(config.vantiq_url, config.vantiq_token, 1);
    char *rsURI = vme_build_custom_rsuri(vme, "VME_Test", NULL);

    vme_result_t *result;
    // projection
    {
        result = vme_select(vme, rsURI, "[\"last_name\", \"first_name\", \"ssn\"]", NULL, NULL, 0, 100);
        if (result->vme_error_msg != NULL) {
            fprintf(stderr, "select w/projection resulted in err: %s", result->vme_error_msg);
        } else {
            //cJSON *json = cJSON_Parse((const char *)result->vme_json_data);
            //char *pretty = cJSON_Print(json);
            //printf("select of 100 w/projection: %.150s...\n", pretty);
            //free(pretty);
            //cJSON_Delete(json);
        }
        CU_ASSERT_PTR_NULL(result->vme_error_msg);
        vme_free_result(result);
    }

    // select all fields and all instances with a call back
    {
        vme_callback_state(vme, NULL);
        result = vme_select_callback(vme, rsURI, NULL, NULL, NULL, 0, 0, select_callback);
        //printf("selected all instances with callback. result buffer size %zu\n", result->vme_size);
        CU_ASSERT_TRUE(result->vme_size == 0);
        CU_ASSERT_PTR_NULL(result->vme_json_data);
        vme_free_result(result);
    }
    // select count
    int total = 0;
    {
        result = vme_select_count(vme, rsURI, NULL, "{\"salary\" : { \"$gt\" : 200000.0}}", NULL);
        CU_ASSERT_PTR_NULL(result->vme_error_msg);
        CU_ASSERT_TRUE(result->vme_count > 0);
        total = result->vme_count;
        vme_free_result(result);
    }

    // page through results
    {
        result = NULL;
        int perPage = 1000;
        int nPages = total / perPage + ((total % perPage) != 0 ? 1 : 0);
        int page = 0;
        do {
            if (result != NULL)
                vme_free_result(result);

            page++; // pages are 1 - based
            result = vme_select(vme, rsURI, NULL, "{\"salary\" : { \"$gt\" : 200000.0}}", NULL, page, 1000);
            CU_ASSERT_PTR_NULL(result->vme_error_msg);
        } while(result->vme_size > 2); // "[]" empty array of instances
        CU_ASSERT_TRUE(page-1 == nPages);
        vme_free_result(result);
    }
    // select one
    {
        result = vme_select_one(vme, rsURI, "[\"id\", \"dept\"]", "{ \"dept\" : \"Marketing\"}");
        CU_ASSERT_PTR_NULL(result->vme_error_msg);
        cJSON *json = cJSON_Parse((const char *)result->vme_json_data);
        CU_ASSERT_PTR_NOT_NULL_FATAL(json);
        CU_ASSERT_PTR_NOT_NULL_FATAL(json->child);
        CU_ASSERT_PTR_NOT_NULL_FATAL(json->child->child);
        CU_ASSERT_PTR_NOT_NULL_FATAL(json->child->child->next);
        cJSON *dept = json->child->child->next->next;
        // [ {"_id" : "foo", "id" : "bar", dept : "Marketing"} ]
        CU_ASSERT_STRING_EQUAL(dept->string, "dept");
        CU_ASSERT_STRING_EQUAL(dept->valuestring, "Marketing");
        cJSON_Delete(json);
        vme_free_result(result);
    }

    {
        result = vme_select(vme, rsURI, "[\"salary\", \"id\"]", "{\"salary\" : {\"$gt\":245000.0}}", "{\"salary\":-1}", 0, 0);
        CU_ASSERT_PTR_NULL(result->vme_error_msg);

        cJSON *json = cJSON_Parse((const char *)result->vme_json_data);
        double prevSal = 10000000000.0;
        cJSON *instances = json->child;
        // make sure results came back in descending order.
        while (instances != NULL) {
            CU_ASSERT_STRING_EQUAL(instances->child->next->string, "salary");
            CU_ASSERT_TRUE(prevSal >= instances->child->next->valuedouble);
            prevSal = instances->child->next->valuedouble;
            instances = instances->next;
        }
        cJSON_Delete(json);
        vme_free_result(result);
    }

    free(config.vantiq_url);
    free(config.vantiq_token);
    vme_teardown(vme);
    CU_PASS("test selects");
}
