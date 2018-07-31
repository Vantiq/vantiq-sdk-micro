// test_delete.c
//
//  Copyright © 2018 VANTIQ. All rights reserved.

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "CUnit/Basic.h"
#include "vme.h"

void test_deletes() {
    vmeconfig_t config;
    if (vme_parse_config("config.properties", &config) == -1)
        CU_ASSERT_EQUAL(-1, 3);

    VME vme = vme_init(config.vantiq_url, config.vantiq_token, 1);
    char *rsURI = vme_build_custom_rsuri(vme, "VME_Test", NULL);

    vme_result_t *result = vme_delete_count(vme, rsURI, "{ \"salary\" : { \"$gt\": 225000.0 }}");
    CU_ASSERT_PTR_NULL(result->vme_error_msg);
    CU_ASSERT_NOT_EQUAL(result->vme_count, 0);
    vme_free_result(result);

    result = vme_delete(vme, rsURI, NULL);
    CU_ASSERT_PTR_NULL(result->vme_error_msg);
    CU_ASSERT_EQUAL(result->vme_count, 0);
    vme_free_result(result);

    rsURI = vme_build_custom_rsuri(vme, "Discovery", NULL);
    result = vme_delete(vme, rsURI, NULL);
    CU_ASSERT_PTR_NULL(result->vme_error_msg);

    vme_teardown(vme);
    free(config.vantiq_token);
    free(config.vantiq_url);
    CU_PASS("test deletes");
}
