//
// vme.c VANTIQ Micro Edition
//
//  a C-based library providing HTTPS connectivity and access to services
//  provided by the VANTIQ system
//
//  Copyright © 2018 VANTIQ. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vme.h"
#include "utils.h"
#include "vantiq_client.h"

/*
 * vme_init --
 *
 *      url - vantiq server URL. e.g. https://api.vantiq.com
 *      authToken - access token created via Modelo. this should grant "long lived" access to the server namespace
 *          in which it was created.
 *      apiVersion - the version of the REST API that the server supports.
 *
 * fire up an instance of VME. this amounts to trying the url / access token combination and see if we can successfully
 * authenticate to the VANTIQ server. Most (all) of that activity takes place in the vantiq client code. the role of
 * vme_init is to wrap up the vantiq client data in an opaque "handle" that is required for all subsequent access.
 */
VME vme_init(const char *url, const char *authToken, uint8_t apiVersion)
{
    vantiq_client_t *vc = vc_init(url, authToken, apiVersion);
    return (VME)vc;
}

/*
 * vme_teardown --
 *
 *      vme - the handle to tear down.
 *
 * this is how the app declares it is done dealing with VANTIQ. the function deallocates all resources associated with
 * connection being closed down. upon return the VME handle can no longer be used with any of the library entry points
 * mostly this means shutting down the associated CURL handle (which will close out any lingering open connections) and
 * freeing up allocated memory.
 */
void vme_teardown(VME vme)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc != NULL)
        vc_teardown(vc);
}

/*
 * vme_error_result --
 *
 *      errMsg - the error message conveying all the salient details around the detected error.
 *
 * this is a helper function that we don't expose to developers. typically, if we detect that an entry point was called
 * with an invalidate VME handle, we'll call this to construct an informative error (i.e. you blew it).
 */
vme_result_t *vme_error_result(const char *errMsg)
{
    vme_result_t *err = malloc(sizeof(vme_result_t));
    memset(err, 0, sizeof(vme_result_t));
    err->vme_error_msg = strdup(errMsg);
    return err;
}

/*
 * vme_callback_state --
 *
 *      VME  - handle returned from call to vme_init
 *      state - user defined / user managed state taht we pass to each callback invocation upon data receipt from the
 *          server. this state is only relevant to the select API that accepts a callback function. otherwise we ignore
 *          it.
 */
void vme_callback_state(VME vme, void *state)
{

    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc != NULL) {
        vc->callback_state = state;
    }
    // silently fail if we must
}

/*
 * _select --
 *
 *      vme = handle returned from a call to vme_init
 *      rsURI - a path to the resource we are selecting from
 *      params - all query parameters neatly packaged up into a singly linked list. this could include a where clause
 *          sort specification, properties to project, etc. whatever select allows...
 *
 *      internal "work horse" function for dealing with select requests.
 */
static vme_result_t *_select(VME vme, const char *rsURI, struct param *params)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME handle");

    vme_result_t *result = vc_get(vc, rsURI, params);
    return result;
}

/*
 * vme_select_callback --
 *
 *      vme - handle returned from call to vme_init
 *      rsURI - path to the resource we are selecting from
 *      propSpecs - a specification of properties of interest (what we return from the select query)
 *      where - where clause against the resource restricting the results that come back
 *      sortSpec - how to order the results.
 *      page / limit - a query cursor. you can ask the server for results one page at a time. when a page
 *          is given, you get the next 'limit' worth of results. when page is not given (set to 0) you get exactly
 *          'limit' results and no more.
 *
 *          e.g. page / limit:  1/1000, 2/1000, 3/1000 in 3 separate calls gives you 3 consecutive result sets of 1000
 *          instances (assuming the result set has 3000 or more results in it). once you hit the end of the result set
 *          the call returns an empty JSON array: "[]"
 *
 *      callback - a function pointer that we invoke as each chunk of "raw" data is returned from the server. the chunks
 *          do not respect JSON object boundaries in any way and are just a chunk of bytes
 */
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
        return vme_error_result("invalid VME handle");
    vc->recv_callback = callback;
    vme_result_t *result = vme_select(vme, rsURI, propSpecs, where, sortSpec, page, limit);
    vc->recv_callback = NULL;
    return result;
}

/*
 * build_select_params --
 *
 *      propSpecs - list of properties for projection
 *      where - where clause restriction on results
 *      sortSpec - specification for how the result set should be ordered
 *      page - which page of results to return
 *      limit - total number of results per page, or total number of results if no page given.
 *
 *  bundle up the various options for a select call into a singly linked list of parameters suitable for
 *  transmogrification into URL parameters.
 */
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

/*
 * vme_select_count
 *
 *      vme -handle returned from call to vme_init
 *      rsURI - path to the resource we are selecting from
 *      propSpecs - a specification of properties of interest (what we return from the select query)
 *      where - where clause against the resource restricting the results that come back
 *      sortSpec - how to order the results.
 *
 *  this function operates just like a regular select w,r.t to the result set. In addition to the actual results it
 *  returns a count of the total results in the result meta data. see vme_result_t::vme_count
 */
vme_result_t *vme_select_count(VME vme, const char *rsURI, const char *propSpecs, const char *where, const char *sortSpec)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME handle");

    struct param *params = build_select_params(propSpecs, where, sortSpec, 0, 0);
    params = build_param(params, "count", "true");
    vme_result_t *result = _select(vme, rsURI, params);
    free_params(params);
    return result;
}

/*
 * vme_select --
 *
 *      vme -handle returned from call to vme_init
 *      rsURI - path to the resource we are selecting from
 *      propSpecs - a specification of properties of interest (what we return from the select query)
 *      where - where clause against the resource restricting the results that come back
 *          for more information on select see the reference: https://api.vantiq.com/docs/system/api/index.html#select
 *          for more information on the where parameter: https://api.vantiq.com/docs/system/api/index.html#where-parameter
 *      sortSpec - how to order the results.
 *      page / limit - a poor persons query cursor. you can ask the server for results one page at a time. when a page
 *          is given, you get the next 'limit' worth of results. when page is not given (set to 0) you get exactly
 *          'limit' results and no more.
 *
 *          e.g. page / limit:  1/1000, 2/1000, 3/1000 in 3 separate calls gives you 3 consecutive result sets of 1000
 *          instances (assuming the result set has 3000 or more results in it). once you hit the end of the result set
 *          the call returns an empty array: "[]"
 *
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

/*
 * _delete -- work horse function for the various delete API entry points.
 *
 *      vme -handle returned from call to vme_init
 *      rsURI - path to the resource we are selecting from
 *      where - where clause against the resource restricting the results that come back
 *      count - place a count of the instances deleted in the result
 */
static vme_result_t *_delete(VME vme, const char *rsURI, const char *where, int count)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME handle");
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

/*
 * vme_delete_count -- delete instances indicated by the rsURI and where clause providing a count of the deleted instances in
 *      the result
 *
 *      vme -handle returned from call to vme_init
 *      rsURI - path to the resource we are selecting from
 *      where - where clause against the resource restricting the results that come back
 */
vme_result_t *vme_delete_count(VME vme, const char *rsURI, const char *where)
{
    return _delete(vme, rsURI, where, 1);
}

/*
 * vme_delete -- delete instances indicated by the rsURI and where clause
 *
 *      vme -handle returned from call to vme_init
 *      rsURI - path to the resource we are selecting from
 *      where - where clause against the resource restricting the results that come back
 */
vme_result_t *vme_delete(VME vme, const char *rsURI, const char *where)
{
    return _delete(vme, rsURI, where, 0);
}

/*
 * vme_select_one --
 *
 *      vme -handle returned from call to vme_init
 *      rsURI - path to the resource we are selecting from
 *      props - a specification of properties of interest (what we return from the select query)
 *      where - where clause against the resource restricting the results that come back
 *
 * select one instance from the given resource type. Note that the resource could either be
 * system or user defined. it is up to the caller to supply the correct path
 * for the desired rs. e.g. /resources/custom/a vs. /resources/types/a
 */
vme_result_t *vme_select_one(VME vme, const char *rsURI, const char *props, const char *where)
{
    return vme_select(vme, rsURI, props, where, NULL, 0, 1);
}

static vme_result_t *_insert(VME vme, const char *rsURI, const char *json, const size_t size, struct param *params)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME handle");
    
    vmebuf_t *msg = vmebuf_ensure_size(NULL, size);
    vmebuf_concat(msg, json, size);
    vme_result_t *result = vc_post(vc, rsURI, msg, params);
    vmebuf_dealloc(msg);
    return result;
}

/*
 * vme_insert --
 *
 *      vme - handle returned from call to vme_init
 *      rsURI - path to the resource we are inserting into
 *      json - an array of JSON formatted instances "[{ first_name : "Barry", last_name : "Obama", ... }]
 *      size - size of the json data
 *
 * insert instances into the specified type.
 */
vme_result_t *vme_insert(VME vme, const char *rsURI, const char *json, const size_t size)
{
    return _insert(vme, rsURI, json, size, NULL);
}

/*
 * vme_update --
 *
 *      vme - handle returned from call to vme_init
 *      rsURI - path to the resource we are inserting into
 *      json - specification of properties and values to update: [{ first_name : "Barak"}]
 *      size - size of the json data
 */
vme_result_t *vme_update(VME vme, const char *rsURI, const char *json, const size_t size)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME handle");
    vmebuf_t *msg = vmebuf_ensure_size(NULL, size);
    vmebuf_concat(msg, json, size);
    vme_result_t *result = vc_put(vc, rsURI, msg, NULL);
    vmebuf_dealloc(msg);
    return result;
}

/*
 * vme_upsert --
 *
 *      vme - handle returned from call to vme_init
 *      rsURI - path to the resource we are inserting into
 *      json - specification of an entire instance in json format.
 *      size - size of the json data
 *
 *  instance specified is either inserted into the type or if already present updated by the given property values
 */
vme_result_t *vme_upsert(VME vme, const char *rsURI, const char *json, size_t size)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME handle");
    vmebuf_t *msg = vmebuf_ensure_size(NULL, size);
    vmebuf_concat(msg, json, size);
    struct param *params = build_param(NULL, "upsert", "true");
    vme_result_t *result = vc_post(vc, rsURI, msg, params);
    free_params(params);
    vmebuf_dealloc(msg);
    return result;
}

/*
 * vme_publish --
 *
 *      vme - handle returned from call to vme_init
 *      topic - any path / topic of interest to the app. the system doesn't require it to be predefined
 *      json - JSON formatted data to publish
 *      size - size of the data.
 */
vme_result_t *vme_publish(VME vme, const char *topic, const char *json, size_t size)
{
    vmebuf_t *msg = vmebuf_ensure_size(NULL, size);
    vmebuf_concat(msg, json, size);

    vantiq_client_t *vc = vc_from_vme(vme);
    /* TODO: i18n */
    if (vc == NULL)
        return vme_error_result("invalid VME handle");
    char *rsURI = vme_build_system_rsuri(vme, TOPICS, topic, NULL);
    vme_result_t *result = vc_post(vc, rsURI, msg, NULL);
    free(rsURI);
    vmebuf_dealloc(msg);
    return result;
}

/*
 * vme_free_result --
 *
 *      result - allocated data previously returned from a vme API call
 *
 * free up all allocated resources associated with the result set.
 */
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

/*
 * vme_patch --
 *
 *      vme - handle returned from call to vme_init
 *      rsURI - path to the resource we are inserting into
 *      json - specification of what data in the type to "patch".
 *
 * for patch details see : http://jsonpatch.com/
 */
vme_result_t *vme_patch(VME vme, const char *rsURI, const char *json)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return NULL;
    return vc_patch(vc, rsURI, json);
}

/*
 * vme_build_custom_rsuri --
 *
 *      vme - handle returned from call to vme_init
 *      rsName - flat name of the resource e.g. "MyType" or "Invoice" etc.
 *      rsID - optionally NULL id for a particular resource instance.
 *
 *      RETURN an allocated string containing the full path to the specified user defined resource:
 *          /resources/custom/<rs name>[/<rs id>]
 *
 * there is extensive documentation for resource URIs at https://api.vantiq.com/docs/system/resourceguide/index.html
 */
char *vme_build_custom_rsuri(VME vme, const char *rsName, const char *rsID)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return NULL;
    return build_rsuri(vc->api_version, vme_mapto_rsname(CUSTOM), rsName, rsID);
}

/*
 * vme_build_system_rsuri --
 *
 *      vme - handle returned from call to vme_init
 *      systype - the type of the system resource e.g. TOPICS
 *      rsID - optionally NULL id for a particular resource instance.
 *
 *      RETURN an allocated string containing the full path to the specified user defined resource:
 *          /resources/topics/a/b/c
 *
 * there is extensive documentation for resource URIs at https://api.vantiq.com/docs/system/resourceguide/index.html
 * REST API specific resource URI details afer here: https://api.vantiq.com/docs/system/api/index.html#rest-over-http-binding
 */
char *vme_build_system_rsuri(VME vme, vme_rstype_t systype, const char *rsName, const char *rsID)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return NULL;
    return build_rsuri(vc->api_version, vme_mapto_rsname(systype), rsName, rsID);
}

/*
 * vme_query_source --
 *
 *      Performs a query against the specified source. The parameters for the
 *      query are provided in the qParams parameter as a JSON document
 */
vme_result_t *vme_query_source(VME vme, const char *sourceID, const char *qParams)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return vme_error_result("invalid VME handle");
    size_t len = sizeof("/sources/query/") + strlen(sourceID);
    char *sourceURI = malloc(len);
    snprintf(sourceURI, len, "/sources/%s/query", sourceID);
    vme_result_t *result = vc_query(vc, sourceURI, qParams);
    free(sourceURI);
    return result;
}

/*
 * vme_aggregate --
 *
 *      vme -handle returned from call to vme_init
 *      rsURI - path to the resource we are selecting from
 *      pipeline - json specification for the pipeline
 *      [{ $match: { salary : { $gt : 200000.0} }}, { $group: { _id: $dept, total: { $sum: $salary}, average: { $avg : $salary }, max: { $max : $salary }, min: { $min : $salary }}} ]
 *
 * execute the aggregate specified in the pipeline. aggregate pipelines are discussed in some detail in the mongodb
 * documentation. https://docs.mongodb.com/manual/aggregation
 */
vme_result_t *vme_aggregate(VME vme, const char *rsURI, const char *pipeline)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return vme_error_result("invalid VME handle");
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

/*
 * vme_execute --
 *
 *      procID - the ID of the procedure to execute
 *      argsDoc - JSON specifying the parameters and values to pass in:
 *          {\"empSSN\": \"655-71-9041\", \"newSalary\": 500000.00}
 *
 * function builds the right resource path and asks the server to invoke the procedure. resulting json is passed back
 * in the return results.
 */
vme_result_t *vme_execute(VME vme, const char *procID, const char *argsDoc)
{
    vantiq_client_t *vc = vc_from_vme(vme);
    if (vc == NULL)
        return vme_error_result("invalid VME handle");
    char *rsURI = vme_build_system_rsuri(vme, PROCEDURES, procID, NULL);
    vme_result_t *result = vc_execute(vc, rsURI, argsDoc);
    free(rsURI);
    return result;
}
