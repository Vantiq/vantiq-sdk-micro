#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "CUnit/Basic.h"
#include "vme.h"
#include "cjson.h"
#include "vme_test.h"

void test_patch()
{
    vmeconfig_t config;
    vme_parse_config("config.properties", &config);
    VME vme = vme_init(config.vantiq_url, config.vantiq_token, 1);

    // patch a Discovery document
    {
        // find an id to patch
        char *rsURI = vme_build_custom_rsuri(vme, "Discovery", NULL);
        vme_result_t *result = vme_select_one(vme, rsURI, NULL, NULL);
        CU_ASSERT_PTR_NOT_NULL_FATAL(result->vme_json_data);
        char *id = find_instance_id(result);
        CU_ASSERT_PTR_NOT_NULL_FATAL(id);
        free(rsURI);
        vme_free_result(result);


        rsURI = vme_build_custom_rsuri(vme, "Discovery", id);
        const char *patchSpec = "[{\"op\": \"replace\", \"path\": \"/devices/glossary/GlossDiv/GlossList/GlossEntry/GlossTerm\", \"value\": \"replaced GlossTerm\"}]";
        result = vme_patch(vme, rsURI, patchSpec);
        CU_ASSERT_PTR_NULL(result->vme_error_msg);
        CU_ASSERT_TRUE(strstr(result->vme_json_data, "replaced GlossTerm") != NULL);
        free(id);
        free(rsURI);
        vme_free_result(result);
    }
    free(config.vantiq_url);
    free(config.vantiq_token);
    vme_teardown(vme);
}
