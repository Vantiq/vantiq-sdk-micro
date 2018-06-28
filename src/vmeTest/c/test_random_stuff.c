//
//  test_random_stuff.c
//  vipo
//
//  Created by Jeffrey Meredith on 6/26/18.
//  Copyright © 2018 Jeffrey Meredith. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjson.h"
#include "vme.h"
#include "vme_test.h"

void test_random_stuff()
{
    char *rsURI;
    const char *instance;
    cJSON *json;

    vmeconfig_t config;
    vme_parse_config("config.properties", &config);
    VME vme = vme_init(config.vantiq_url, config.vantiq_token, 1);
    rsURI = vme_build_custom_rsuri(vme, "ChinaU", NULL);
    vme_result_t *result = vme_select_one(vme, rsURI, NULL, NULL);
    const char *chinaUnicomID = find_instance_id(result);
    free(rsURI);
    vme_free_result(result);

    if (chinaUnicomID != NULL) {
        rsURI = vme_build_custom_rsuri(vme, "ChinaU", chinaUnicomID);
        const char *patchSpec = "[{\"op\": \"replace\", \"path\": \"/doc/glossary/GlossDiv/GlossList/GlossEntry/GlossTerm\", \"value\": \"replaced GlossTerm\"}]";
        result = vme_patch(vme, rsURI, patchSpec);
        if (result->vme_error_msg == NULL) {
            json = cJSON_Parse((const char *)result->vme_json_data);
            printf("after PATCH, china unicom instance is:\n%s\n", cJSON_Print(json));
            cJSON_Delete(json);
        }
    } else {
        fprintf(stderr, "failed to find chinaU instance _id, skipping PATCH test");
    }

    rsURI = vme_build_custom_rsuri(vme, "Sensors", NULL);
    result = vme_select(vme, rsURI, NULL, NULL, "{\"PumpID\":1}", 0, 3);
    json = cJSON_Parse((const char *)result->vme_json_data);
    printf("selected the following from the Sensors type ordered by PumpID: %s ...\n", cJSON_Print(json));
    cJSON_Delete(json);
    free(rsURI);
    vme_free_result(result);

    rsURI = vme_build_custom_rsuri(vme, "Invoice", NULL);
    result = vme_select(vme, rsURI, "[\"amount\",\"lastName\",\"timestamp\",\"ID\"]", NULL, "{\"amount\":-1}", 0, 10);
    json = cJSON_Parse((const char *)result->vme_json_data);
    printf("selected the following projection from Invoice type order desc amount: %s ...\n", cJSON_Print(json));
    cJSON_Delete(json);
    free(rsURI);
    vme_free_result(result);

    rsURI = vme_build_custom_rsuri(vme, "Invoice", NULL);
    result = vme_select(vme, rsURI, "[\"amount\",\"lastName\",\"timestamp\",\"ID\"]", "{\"amount\": {\"$gt\":20.0}}", "{\"amount\":-1}", 0, 10);
    if (result->vme_error_msg == NULL) {
        json = cJSON_Parse((const char *)result->vme_json_data);
        printf("selected the following 10 projected instances from the Invoice type sorted by amount where amount > 20.0: %s ...\n", cJSON_Print(json));
        cJSON_Delete(json);
    } else {
        fprintf(stderr, "error inserting to Invoice: %s\n", result->vme_error_msg);
    }
    fflush(stdout); fflush(stderr);
    free(rsURI);
    vme_free_result(result);

    /* insert a badly formatted instance -- should error */
    rsURI = vme_build_custom_rsuri(vme, "Invoice", NULL);
    instance = "{\"amount\" : 711.69 \"description\" : \"happy bday\" \"firstName\" : \"Jeffrey\" \"ID\" : \"deadbeef-cafe-4645-844d-e70c7fc76961\" \"lastName\" : \"Meredith\" \"timestamp\" : \"2018-06-20T05:49:23.208Z\"}";
    result = vme_insert(vme, rsURI, instance, strlen(instance));
    fprintf(stderr, "inserted an badly formatted instance to Invoice: %s\n", result->vme_error_msg);
    fflush(stdout); fflush(stderr);
    free(rsURI);
    vme_free_result(result);

    /* insert a single instance */
    rsURI = vme_build_custom_rsuri(vme, "Invoice", NULL);
    instance = "{\"amount\" : 711.69, \"description\" : \"happy bday\", \"firstName\" : \"Jeffrey\", \"ID\" : \"9bbeefce-e8ed-4645-844d-e70c7fc76961\", \"lastName\" : \"Meredith\", \"timestamp\" : \"2018-06-20T05:49:23.208Z\"}";
    result = vme_insert(vme, rsURI, instance, strlen(instance));
    printf("inserted an instance to Invoice\n");
    const char *_id = NULL;
    if (result->vme_error_msg == NULL) {
        _id = find_instance_id(result);
    }
    fflush(stdout); fflush(stderr);
    free(rsURI);
    vme_free_result(result);

    if (_id != NULL) {
        rsURI = vme_build_custom_rsuri(vme, "Invoice", _id);
        instance = "{\"amount\" : 711.69, \"description\" : \"you're too old!\", \"firstName\" : \"Jeffrey\", \"ID\" : \"9bbeefce-e8ed-4645-844d-e70c7fc76961\", \"lastName\" : \"Meredith\", \"timestamp\" : \"2018-06-20T05:49:23.208Z\"}";
        result = vme_update(vme, rsURI, instance, strlen(instance));
        if (result->vme_error_msg == NULL) {
            find_instance_id(result);
            printf("updated an instance to Invoice\n");
        }
        free(rsURI);
        vme_free_result(result);
    } else {
        fprintf(stderr, "insert failed, skipping update of that instance\n");
    }
    fflush(stdout); fflush(stderr);

    if (_id != NULL) {
        rsURI = vme_build_custom_rsuri(vme, "Invoice", _id);
        result = vme_delete(vme, rsURI, NULL);
        if (result->vme_error_msg == NULL) {
            find_instance_id(result);
            printf("deleted an instance to Invoice\n");
        }
        free(rsURI);
        vme_free_result(result);
    } else {
        fprintf(stderr, "delete failed, skipping update of that instance\n");
    }
    fflush(stdout); fflush(stderr);

    /* insert two instances in a single request */
    rsURI = vme_build_custom_rsuri(vme, "Invoice", NULL);
    instance = "[{\"amount\" : 218.99, \"description\" : \"happy bday\", \"firstName\" : \"Andrew\", \"ID\" : \"8adeefce-e8ed-4645-844d-e70c7fc76961\", \"lastName\" : \"Meredith\", \"timestamp\" : \"2018-06-02T03:49:23.208Z\"},{\"amount\" : 224.01, \"description\" : \"happy bday\", \"firstName\" : \"Liam\", \"ID\" : \"7abeefce-e8ed-4645-844d-e70c7fc76961\", \"lastName\" : \"Zhu\", \"timestamp\" : \"2018-05-02T13:49:23.208Z\"}]";
    result = vme_insert(vme, rsURI, instance, strlen(instance));
    if (result->vme_error_msg == NULL) {
        json = cJSON_Parse((const char *)result->vme_json_data);
        printf("inserted 2 instances to Invoice:\n%s\n",  cJSON_Print(json));
        cJSON_Delete(json);
    } else {
        fprintf(stderr, "attempt to insert 2 instances to Invoice failed\n");
    }
    fflush(stdout); fflush(stderr);
    free(rsURI);
    vme_free_result(result);

    rsURI = vme_build_custom_rsuri(vme, "Invoice", NULL);
    result = vme_delete_count(vme, rsURI, "{ \"$or\" : [{\"amount\" : 218.99},{\"amount\" : 224.01}] }");
    if (result->vme_error_msg == NULL) {
        json = cJSON_Parse((const char *)result->vme_json_data);
        printf("deleted %u instances of Invoice:\n%s\n",  result->vme_count, cJSON_Print(json));
        cJSON_Delete(json);
    }
    free(rsURI);
    vme_free_result(result);

    /* upsert two instances in a single request */
    rsURI = vme_build_custom_rsuri(vme, "Invoice", NULL);
    instance = "[{\"amount\" : 218.99, \"description\" : \"happy bday\", \"firstName\" : \"Andrew\", \"ID\" : \"8adeefce-e8ed-4645-844d-e70c7fc76961\", \"lastName\" : \"Meredith\", \"timestamp\" : \"2018-06-02T03:49:23.208Z\"},{\"amount\" : 224.01, \"description\" : \"happy bday\", \"firstName\" : \"Liam\", \"ID\" : \"7abeefce-e8ed-4645-844d-e70c7fc76961\", \"lastName\" : \"Zhu\", \"timestamp\" : \"2018-05-02T13:49:23.208Z\"}]";
    result = vme_upsert(vme, rsURI, instance, strlen(instance));
    if (result->vme_error_msg == NULL) {
        json = cJSON_Parse((const char *)result->vme_json_data);
        printf("upserted 2 instances to Invoice:\n%s\n",  cJSON_Print(json));
        cJSON_Delete(json);
    } else {
        fprintf(stderr, "attempt to insert 2 instances to Invoice failed\n");
    }
    fflush(stdout); fflush(stderr);
    vme_free_result(result);

    instance = "[{\"amount\" : 711.69, \"description\" : \"happy bday\", \"firstName\" : \"Andrew\", \"ID\" : \"8adeefce-e8ed-4645-844d-e70c7fc76961\", \"lastName\" : \"Meredith\", \"timestamp\" : \"2018-06-02T03:49:23.208Z\"},{\"amount\" : 701.70, \"description\" : \"happy bday\", \"firstName\" : \"Liam\", \"ID\" : \"7abeefce-e8ed-4645-844d-e70c7fc76961\", \"lastName\" : \"Zhu\", \"timestamp\" : \"2018-05-02T13:49:23.208Z\"}]";
    result = vme_upsert(vme, rsURI, instance, strlen(instance));
    if (result->vme_error_msg == NULL) {
        json = cJSON_Parse((const char *)result->vme_json_data);
        printf("upserted 2 instances to Invoice:\n%s\n",  cJSON_Print(json));
        cJSON_Delete(json);
    } else {
        fprintf(stderr, "attempt to insert 2 instances to Invoice failed\n");
    }
    fflush(stdout); fflush(stderr);
    vme_free_result(result);

    /* try an aggregate over the Invoice type */
    const char *pipeline = "[{\"$match\": { \"amount\" : { \"$gt\" : 200.0} }}, { \"$group\": { \"_id\": \"$ID\", \"total\": { \"$sum\": \"$amount\"}}} ]";
    result = vme_aggregate(vme, rsURI, pipeline);
    if (result->vme_error_msg == NULL) {
        json = cJSON_Parse((const char *)result->vme_json_data);
        printf("Aggregate pipeline on Invoice:\n%s\n",  cJSON_Print(json));
        cJSON_Delete(json);
    } else {
        fprintf(stderr, "aggregate pipeline against Invoice failed with %s\n", result->vme_error_msg);
    }
    fflush(stdout); fflush(stderr);
    free(rsURI);
    vme_free_result(result);
}
