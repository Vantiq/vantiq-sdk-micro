//
//  test_aggregate.c
//
//  Copyright © 2018 VANTIQ. All rights reserved.

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "CUnit/Basic.h"
#include "vme.h"
#include "cjson.h"
#include "vme_test.h"

void test_aggregates()
{
    vmeconfig_t config;
    vme_parse_config("config.properties", &config);
    VME vme = vme_init(config.vantiq_url, config.vantiq_token, 1);

    // find an instance to update
    {
        char *rsURI = vme_build_custom_rsuri(vme, "VME_Test", NULL);
        const char *pipeline = "[{\"$match\": { \"salary\" : { \"$gt\" : 200000.0} }}, { \"$group\": { \"_id\": \"$dept\", \"total\": { \"$sum\": \"$salary\"}, \"average\": { \"$avg\" : \"$salary\" }, \"max\": { \"$max\" : \"$salary\" }, \"min\": { \"$min\" : \"$salary\" }}}, { \"$match\": { \"average\": { \"$gte\": 225000.0 }}} ]";
        vme_result_t *result = vme_aggregate(vme, rsURI, pipeline);
        CU_ASSERT_PTR_NULL(result->vme_error_msg);
        vme_free_result(result);

        pipeline = "[ {\"$match\": { \"salary\" : { \"$gt\" : 200000.0} }}, { \"$group\": { \"_id\": \"$dept\", \"total\": { \"$sum\": \"$salary\"}, \"average\": { \"$avg\" : \"$salary\" }, \"max\": { \"$max\" : \"$salary\" }, \"min\": { \"$min\" : \"$salary\" }}} ]";
        result = vme_aggregate(vme, rsURI, pipeline);
        CU_ASSERT_PTR_NULL(result->vme_error_msg);

        pipeline = "[{ \"$group\": { \"_id\": \"$dept\", \"total\": { \"$sum\": \"$salary\"}, \"average\": { \"$avg\" : \"$salary\" }, \"max\": { \"$max\" : \"$salary\" }, \"min\": { \"$min\" : \"$salary\" }}} ]";
        result = vme_aggregate(vme, rsURI, pipeline);
        CU_ASSERT_PTR_NULL(result->vme_error_msg);

        vme_free_result(result);
    }

    free(config.vantiq_token);
    free(config.vantiq_url);
    vme_teardown(vme);
}
