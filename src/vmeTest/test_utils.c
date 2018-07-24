//
//  test_utils.c
//
//  Created by Jeffrey Meredith on 6/26/18.
//  Copyright © 2018 Jeffrey Meredith. All rights reserved.
//
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "cjson.h"
#include "vme_test.h"

cJSON *_find_instance_prop(cJSON *instance, const char *propName, int remove);

/*
 * given a user defined type instance object, find it's system generated id
 */
char *find_instance_id(vme_result_t *result)
{
    cJSON *json = cJSON_Parse((const char *)result->vme_json_data);
    //printf("searching for instance ID in object:\n%s\n", cJSON_Print(json));
    cJSON *instanceId = find_instance_prop(json, "_id");
    char *idStr = NULL;
    if (instanceId != NULL) {
        idStr = strdup(instanceId->valuestring);
    }
    cJSON_Delete(json);
    return idStr;
}

cJSON *_find_instance_prop(cJSON *instance, const char *propName, int remove)
{
    //printf("searching for %s in object:\n%s\n", propName, cJSON_Print(instance));
    fflush(stdout);
    cJSON *parent;
    if (instance->type == cJSON_Array) {
        parent = instance->child;
    }
    else {
        parent = instance;
    }
    assert(parent->type == cJSON_Object);
    cJSON *prop = parent->child;
    cJSON *prevProp = NULL;
    while (prop != NULL && strcmp(prop->string, propName) != 0) {
        prevProp = prop;
        prop = prop->next;
    }
    if (remove && prop != NULL) {
        if (prevProp == NULL) {
            parent->child = prop->next;
        } else {
            prevProp->next = prop->next;
        }
    }
    return prop;
}

cJSON *remove_prop(cJSON *instance, const char *propName)
{
    cJSON *prop = _find_instance_prop(instance, propName, 1);
    free(prop);
    return instance;
}

cJSON *find_instance_prop(cJSON *instance, const char *propName)
{
    return _find_instance_prop(instance, propName, 0);
}
