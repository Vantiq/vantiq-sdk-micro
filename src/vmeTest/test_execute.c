// test_execute.c
//
//  Copyright © 2018 VANTIQ. All rights reserved.

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>

#include "CUnit/Basic.h"
#include "vme.h"
#include "cjson.h"
#include "vme_test.h"

void test_execute()
{
    vmeconfig_t config;
    vme_parse_config("config.properties", &config);
    VME vme = vme_init(config.vantiq_url, config.vantiq_token, 1);

    {
        vme_result_t *result = vme_execute(vme, "VME_Test_Proc", "{\"empSSN\": \"655-71-9041\", \"newSalary\": 500000.00}");
        CU_ASSERT_PTR_NULL(result->vme_error_msg);
        //CU_ASSERT_PTR_NOT_NULL(result->vme_json_data); Not true even on success
        vme_free_result(result);

        char *rsURI = vme_build_custom_rsuri(vme, "VME_Test", NULL);
        rsURI = vme_build_custom_rsuri(vme, "VME_Test", NULL);
        result = vme_select_one(vme, rsURI, "[\"salary\"]", "{ \"ssn\" : \"655-71-9041\"}");
        result = vme_select_one(vme, rsURI, "[\"salary\"]", "{ \"ssn\" : \"655-71-9041\"}");
        CU_ASSERT_PTR_NULL(result->vme_error_msg);
        CU_ASSERT_PTR_NOT_NULL(result->vme_json_data);

        cJSON *json = cJSON_Parse(result->vme_json_data);
        cJSON *salary = find_instance_prop(json, "salary");
        CU_ASSERT_TRUE(salary->valuedouble == 500000.00);
        free(rsURI);
        cJSON_Delete(json);
        vme_free_result(result);
    }

    free(config.vantiq_token);
    free(config.vantiq_url);
    vme_teardown(vme);
}
