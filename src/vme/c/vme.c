#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vme.h"
#include "utils.h"
#include "vantiq_client.h"

/*
 */
VME vme_init(const char *url, const char *authToken, uint8_t apiVersion)
{
    vantiq_client_t *vc = vc_init(url, authToken, apiVersion);
    return (VME)vc;
}

/*
 */
void vme_teardown(VME vme)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc != NULL)
        vc_teardown(vc);
}

vme_result_t *vme_error_result(const char *errMsg)
{
    vme_result_t *err = malloc(sizeof(vme_result_t));
    memset(err, 0, sizeof(vme_result_t));
    err->vme_error_msg = strdup(errMsg);
    return err;
}

void vme_callback_state(VME vme, void *state)
{

    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc != NULL) {
        vc->callback_state = state;
    }
}

vme_result_t *_select(VME vme, const char *rsURI, struct param *params)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME descriptor");

    vme_result_t *result = vc_get(vc, rsURI, params);
    return result;
}

vme_result_t *vme_select_callback(VME vme,
                                  const char *rsURI, // fully qualified URI to a resource
                                  const char *propSpecs, // projected properties [ "prop1", prop2, ... ]
                                  const char *where,
                                  const char *sortSpec,
                                  int page,          // <= 0 means no paging
                                  int limit,        // <= 0 means no limit
                                  size_t (*callback)(void *state, const char *data, size_t size))
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME descriptor");
    vc->recv_callback = callback;
    vme_result_t *result = vme_select(vme, rsURI, propSpecs, where, sortSpec, page, limit);
    vc->recv_callback = NULL;
    return result;
}

struct param *build_select_params(const char *propSpecs, const char *where, const char *sortSpec, int page, int limit)
{
    char buf[32];
    struct param *params = NULL;
    if (propSpecs != NULL) {
        struct param *p = build_param(params, "props", propSpecs);
        params = p;
    }
    if (where != NULL) {
        struct param *p = build_param(params, "where", where);
        params = p;
    }
    if (sortSpec != NULL) {
        params = build_param(params, "sort", sortSpec);
    }
    if (page > 0) {
        sprintf(&buf[0], "%d", page);
        params = build_param(params, "page", &buf[0]);
    }
    if (limit > 0) {
        sprintf(&buf[0], "%d", limit);
        params = build_param(params, "limit", &buf[0]);
    }
    return params;
}

vme_result_t *vme_select_count(VME vme, const char *rsURI, const char *propSpecs, const char *where, const char *sortSpec)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME descriptor");

    struct param *params = build_select_params(propSpecs, where, sortSpec, 0, 0);
    params = build_param(params, "count", "true");
    vme_result_t *result = _select(vme, rsURI, params);
    free_params(params);
    return result;
}

/*
 * SELECT data from the given resource type. Note that the resource could either be
 * system or user defined. it is up to the caller to supply the correct path
 * for the desired rs. e.g. /resources/custom/a vs. /resources/types/a
 */
vme_result_t *vme_select(VME vme,
                         const char *rsURI, // fully qualified URI to a resource
                         const char *propSpecs, // projected properties [ "prop1", prop2, ... ]
                         const char *where,
                         const char *sortSpec,
                         int page,
                         int limit)        // <= 0 means no limit
{
    struct param *params = build_select_params(propSpecs, where, sortSpec, page, limit);
    vme_result_t *result =  _select(vme, rsURI, params);
    free_params(params);
    return result;
}

vme_result_t *_delete(VME vme, const char *rsURI, const char *where, int count)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME descriptor");
    struct param *params = NULL;
    if (where != NULL)
        params = build_param(params, "where", where);
    if (count) {
        params = build_param(params, "count", "true");
    }
    vme_result_t *result = vc_delete(vc, rsURI, params);
    free_params(params);
    return result;
}

vme_result_t *vme_delete_count(VME vme, const char *rsURI, const char *where)
{
    return _delete(vme, rsURI, where, 1);
}

vme_result_t *vme_delete(VME vme, const char *rsURI, const char *where)
{
    return _delete(vme, rsURI, where, 0);
}

vme_result_t *vme_select_one(VME vme, const char *rsURI, const char *props, const char *where)
{
    return vme_select(vme, rsURI, props, where, NULL, 0, 1);
}

static vme_result_t *_insert(VME vme, const char *rsURI, const char *json, const size_t size, struct param *params)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME descriptor");
    
    vmebuf_t *msg = vmebuf_ensure_size(NULL, size);
    vmebuf_concat(msg, json, size);
    vme_result_t *result = vc_post(vc, rsURI, msg, params);
    vmebuf_dealloc(msg);
    return result;
}

vme_result_t *vme_insert(VME vme, const char *rsURI, const char *json, const size_t size)
{
    return _insert(vme, rsURI, json, size, NULL);
}

vme_result_t *vme_update(VME vme, const char *rsURI, const char *json, const size_t size)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME descriptor");
    vmebuf_t *msg = vmebuf_ensure_size(NULL, size);
    vmebuf_concat(msg, json, size);
    vme_result_t *result = vc_put(vc, rsURI, msg, NULL);
    vmebuf_dealloc(msg);
    return result;
}

long vme_insert_count(VME vme, const char *rsURI, char *json, size_t size)
{
    struct param *params = build_param(NULL, "count", "true");
    vme_result_t *result = _insert(vme, rsURI, json, size, params);
    return result->vme_count;
}

vme_result_t *vme_upsert(VME vme, const char *rsURI, const char *json, size_t size)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME descriptor");
    vmebuf_t *msg = vmebuf_ensure_size(NULL, size);
    vmebuf_concat(msg, json, size);
    struct param *params = build_param(NULL, "upsert", "true");
    vme_result_t *result = vc_post(vc, rsURI, msg, params);
    free_params(params);
    vmebuf_dealloc(msg);
    return result;
}

vme_result_t *vme_publish(VME vme, const char *topic, const char *json, size_t size)
{
    vmebuf_t *msg = vmebuf_ensure_size(NULL, size);
    vmebuf_concat(msg, json, size);

    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME descriptor");
    char *rsURI = vme_build_system_rsuri(vme, TOPICS, topic, NULL);
    vme_result_t *result = vc_post(vc, rsURI, msg, NULL);
    free(rsURI);
    vmebuf_dealloc(msg);
    return result;
}

void vme_free_result(vme_result_t *result)
{
    if (result == NULL)
        return;
    if (result->vme_json_data != NULL) {
        free(result->vme_json_data);
        result->vme_json_data = NULL;
    }
    if (result->vme_error_msg != NULL) {
        free(result->vme_error_msg);
        result->vme_error_msg = NULL;
    }
    free(result);
}

vme_result_t *vme_patch(VME vme, const char *rsURI, const char *json)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return NULL;
    return vc_patch(vc, rsURI, json);
}

char *vme_build_custom_rsuri(VME vme, const char *rsName, const char *rsID)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return NULL;
    return build_rsuri(vc->api_version, vme_mapto_rsname(CUSTOM), rsName, rsID);
}

char *vme_build_system_rsuri(VME vme, vme_rstype_t systype, const char *rsName, const char *rsID)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return NULL;
    return build_rsuri(vc->api_version, vme_mapto_rsname(systype), rsName, rsID);
}

vme_result_t *vme_query_source(VME vme, const char *sourceID, const char *qParams)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return vme_error_result("invalid VME descriptor");
    size_t len = sizeof("/sources/query/") + strlen(sourceID);
    char *sourceURI = malloc(len);
    snprintf(sourceURI, len, "/sources/%s/query", sourceID);
    vme_result_t *result = vc_query(vc, sourceURI, qParams);
    free(sourceURI);
    return result;
}

vme_result_t *vme_aggregate(VME vme, const char *rsURI, const char *pipeline)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return vme_error_result("invalid VME descriptor");
    // adjust the URI to perform an aggregate pipeline operation.
    size_t len = strlen(rsURI);
    char *aggRsURI = malloc(len+sizeof("/aggregate"));
    strcpy(aggRsURI, rsURI);
    if (rsURI[len - 1] == '/') {
        strcat(aggRsURI, "aggregate");
    } else {
        strcat(aggRsURI, "/aggregate");
    }
    struct param *params = build_param(NULL, "pipeline", pipeline);
    vme_result_t *result = vc_aggregate(vc, aggRsURI, params);
    free_params(params);
    free(aggRsURI);
    return result;
}

vme_result_t *vme_execute(VME vme, const char *procID, const char *argsDoc)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return vme_error_result("invalid VME descriptor");
    char *rsURI = vme_build_system_rsuri(vme, PROCEDURES, procID, NULL);
    vme_result_t *result = vc_execute(vc, rsURI, argsDoc);
    free(rsURI);
    return result;
}
