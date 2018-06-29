
//  vme.h
//  Vantiq Micro Edition
//
//  a C-based library providing HTTPS connectivity and access to services
//  provided by the VANTIQ system
//
//  Copyright © 2018 VANTIQ. All rights reserved.

#ifndef vantiq_vme_h
#define vantiq_vme_h

#include <stddef.h>
#include <stdint.h>

#define VME_MAX_ERRLEN 1024

typedef void *VME;

typedef struct vme_result {
    size_t      vme_size;
    uint32_t    vme_count;
    char       *vme_json_data;
    char       *vme_error_msg;
} vme_result_t;

typedef enum vantiq_sys_type {
    USERS = 0,
    TYPES = 1,
    NAMESPACES = 2,
    PROFILES = 3,
    SCALARS = 4,
    LOG_MESSAGES = 5,
    DOCUMENTS = 6,
    SOURCES = 7,
    SOURCE_IMPLEMENTATIONS = 8,
    TOPICS = 9,
    RULES = 10,
    CONFIGURATIONS = 11,
    NODES = 12,
    NODE_CONFIGURATIONS = 13,
    COMPONENTS = 14,
    PROCEDURES = 15,
    EVENTS = 16,
    TOKENS = 17,
    ORGANIZATIONS = 18,
    SERIES = 19,
    PRODUCTS = 20,
    AUDITS = 21,
    SITUATIONS = 22,
    PROJECTS = 23,
    GROUPS = 24,
    ACTIVITY_PATTERNS = 25,
    COLLABORATIONS = 26,
    COLLABORATION_TYPES = 27,
    PAGESETS = 28,
    SERVICES = 29,
    CUSTOM = 30,
    DEBUG_CONFIGURATIONS = 31,
    SCHEDULED_EVENTS = 32,
    EVENT_STREAMS = 33,
    DELEGATED_REQUESTS = 34,
    EVENT_DESIGNS = 35,
    VAIL_SCRIPTS = 36
} vme_rstype_t;

/* setup, cleanup, teardown */

/*
 * vme_init requires a VANTIQ server URL and an access token.
 * it connects to the server and authenticates.
 *
 * the access token should be "long lived" and may be obtained from the
 * Operations / Administer menu drop down in the UI.
 *
 * see: https://api.vantiq.com/docs/system/resourceguide/index.html#create-access-token
 *
 * the apiVersion specifies the rev of the REST API the VME library should exptect
 * to work with. 1 is the current built-in assumption.
 */
VME vme_init(const char *url, const char *token, uint8_t apiVersion);
/*
 * vme_teardown should be called when the application no longer needs to
 * connect to the VANTIQ server. Upone return the VME handle returned by the
 * previous call to vme_init will no longer be valid.
 */
void vme_teardown(VME vme);
/*
 * Most calls to VME interfaces return an allocated vme_result_t. Once the
 * application is done processing the results, it should call vme_free_result
 * to deallocate all associated resources with the request.
 */
void vme_free_result(vme_result_t *result);

/*
 * REST API interfaces
 *
 * these calls very closely follow the REST API reference in the documentation.
 * see: https://api.vantiq.com/docs/system/api/index.html
 *
 * and the details for HTTP binding:
 * https://api.vantiq.com/docs/system/api/index.html#rest-over-http-binding
 */
vme_result_t *vme_select_one(VME vme, const char *rsURI, const char *props, const char *where);
vme_result_t *vme_select_count(VME vme, const char *rsURI, const char *propSpecs, const char *where, const char *sortSpec);
vme_result_t *vme_select(VME vme, const char *rsURI, const char *propSpecs, const char *where, const char *sortSpec, int page, int limit);
vme_result_t *vme_select_callback(VME vme, const char *rsURI, const char *propSpecs, const char *where, const char *sortSpec, int page, int limit, size_t (*callback)(void *state, const char *data, size_t size));
vme_result_t *vme_insert(VME vme, const char *rsURI, const char *json, const size_t size);
vme_result_t *vme_update(VME vme, const char *rsURI, const char *json, const size_t size);
vme_result_t *vme_delete(VME vme, const char *rsURI, const char *where);
vme_result_t *vme_delete_count(VME vme, const char *rsURI, const char *where);
vme_result_t *vme_upsert(VME vme, const char *rsURI, const char *json, size_t size);
vme_result_t *vme_patch(VME vme, const char *rsURI, const char *json);
vme_result_t *vme_aggregate(VME vme, const char *rsURI, const char *json);

vme_result_t *vme_publish(VME vme, const char *topic, const char *json, size_t size);
vme_result_t *vme_execute(VME vme, const char *procID, const char *argsDoc);
vme_result_t *vme_query_source(VME vme, const char *sourceID, const char *argsDoc);

/* helper functions */

/*
 * it is possible to register a callback to handle large result sets from a
 * select. the data coming back is CHUNKED by the server and does not respect
 * object boundaries in JSON results. You are basically getting a buffer of bytes
 */
void vme_callback_state(VME vme, void *state);
/*
 * Most of the calls require a resource path indicating which resource you are
 * attempting to access. THere are system resources and "custom" resources or
 * those added by users. vme_build_system_rsuri and vme_build_custom_rsuri help
 * to correctly construct them.
 */
char *vme_build_system_rsuri(VME vme, vme_rstype_t systype, const char *rsName, const char *rsID);
char *vme_build_custom_rsuri(VME vme, const char *rsName, const char *rsID);
/*
 * map a resource type enum to its resource name
 */
const char *vme_mapto_rsname(vme_rstype_t type);


typedef struct
{
    size_t len;        // current length of buffer (used bytes)
    size_t limit;      // maximum length of buffer (allocated)
    char  *data;       // insert bytes here
} vmebuf_t;

#define vmebuf_alloc() vmebuf_ensure_size(NULL, 0)

vmebuf_t *vmebuf_truncate(vmebuf_t *buf);
vmebuf_t *vmebuf_ensure_size(vmebuf_t *buf, size_t len);
vmebuf_t *vmebuf_ensure_incr_size(vmebuf_t *buf, size_t incrsize);

void  vmebuf_push(vmebuf_t *buf, char c);
void  vmebuf_concat(vmebuf_t *buf, const char *data, size_t len);
char *vmebuf_tostr(vmebuf_t *buf);
void  vmebuf_dealloc(vmebuf_t *buf);

typedef struct {
    char *dpi_port;
    char *dpi_socket_path;
    char *vantiq_url;
    char *vantiq_token;
} vmeconfig_t;

int vme_parse_config(const char *path, vmeconfig_t *config);

#endif /* vantiq_vme_h */
