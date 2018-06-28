#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "CUnit/Basic.h"
#include "vme.h"
#include "cjson.h"
#include "vme_test.h"

void test_updates()
{
    vmeconfig_t config;
    vme_parse_config("config.properties", &config);
    VME vme = vme_init(config.vantiq_url, config.vantiq_token, 1);

    // find an instance to update
    char *idStr;
    {
        char *rsURI = vme_build_custom_rsuri(vme, "VME_Test", NULL);
        vme_result_t *result = vme_select_one(vme, rsURI, NULL, NULL);
        cJSON *json = cJSON_Parse((const char *)result->vme_json_data);
        cJSON *id = find_instance_prop(json, "_id");
        CU_ASSERT_PTR_NOT_NULL_FATAL(id);
        idStr = strdup(id->valuestring);
        free(rsURI);
        vme_free_result(result);
        cJSON_Delete(json);
    }

    // update the salary (can't seem to get update field operators to work
    {
        char *rsURI = vme_build_custom_rsuri(vme, "VME_Test", idStr);
        //char *expr = "{ \"salary\": {\"$mul\": { \"salary\" : 1.1}}}";
        char *expr = "{ \"salary\": 251000.00 }";
        vme_result_t *result = vme_update(vme, rsURI, expr, strlen(expr));
        CU_ASSERT_PTR_NULL(result->vme_error_msg);
        vme_free_result(result);
        free(rsURI);
    }

    // change the email and do an upsert (should be an update)
    {
        char *rsURI = vme_build_custom_rsuri(vme, "VME_Test", idStr);
        vme_result_t *result = vme_select(vme, rsURI, NULL, NULL, NULL, 0, 0);
        CU_ASSERT_PTR_NULL_FATAL(result->vme_error_msg);

        cJSON *json = cJSON_Parse((const char *)result->vme_json_data);
        cJSON *email = find_instance_prop(json, "email");
        CU_ASSERT_PTR_NOT_NULL_FATAL(email);

        vme_free_result(result);
        free(email->valuestring);
        email->valuestring = malloc(sizeof("president@whitehouse.gov"));
        strcpy(email->valuestring, "president@whitehouse.gov");
        json = remove_prop(json, "_id");

        char *newInstance = cJSON_Print(json);
        free(rsURI);
        rsURI = vme_build_custom_rsuri(vme, "VME_Test", NULL);
        result = vme_upsert(vme, rsURI, newInstance, strlen(newInstance));
        CU_ASSERT_PTR_NULL(result->vme_error_msg);

        cJSON_Delete(json);
        free(rsURI);
        free(newInstance);
        vme_free_result(result);
    }

    // change the id and do an upsert (should be an insert)
    {
        char *rsURI = vme_build_custom_rsuri(vme, "VME_Test", idStr);
        vme_result_t *result = vme_select(vme, rsURI, NULL, NULL, NULL, 0, 0);
        CU_ASSERT_PTR_NULL_FATAL(result->vme_error_msg);

        cJSON *json = cJSON_Parse((const char *)result->vme_json_data);
        cJSON *id = find_instance_prop(json, "id");
        vme_free_result(result);
        CU_ASSERT_PTR_NOT_NULL_FATAL(id);


        // updating the ID should result in an insert
        free(id->valuestring);
        id->valuestring = malloc(sizeof("xyzzy"));
        strcpy(id->valuestring, "xyzzy");

        json = remove_prop(json, "_id");
        char *newInstance = cJSON_Print(json);
        free(rsURI);
        rsURI = vme_build_custom_rsuri(vme, "VME_Test", NULL);
        result = vme_upsert(vme, rsURI, newInstance, strlen(newInstance));
        CU_ASSERT_PTR_NULL(result->vme_error_msg);

        free(newInstance);
        cJSON_Delete(json);
        free(rsURI);
        vme_free_result(result);
    }
    vme_teardown(vme);
}
