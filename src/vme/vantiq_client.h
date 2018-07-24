#ifndef VANTIQ_CLIENT_H
#define VANTIQ_CLIENT_H

#include <curl/curl.h>
#include "vme.h"

typedef struct vc_sendstate {
    const char *readptr;
    size_t sizeleft;
} vc_sendstate_t;

struct vantiq_client {
    uint8_t            magic;
    uint8_t            api_version;
    uint32_t           result_count;
    CURL              *curl;
    char              *server_url;
    struct curl_slist *http_hdrs;
    vmebuf_t             *recv_buf;
    size_t           (*recv_callback)(void *state, const char *data, size_t size);
    void              *callback_state;
    vc_sendstate_t     send_state;
};

struct param {
    char *key;
    char *value;
    struct param *next;
};

typedef struct vantiq_client vantiq_client_t;

struct param *build_param(struct param *head, const char *key, const char *value);
void free_params(struct param *params);

char *construct_url(vantiq_client_t *vc, const char *url, int nParams, ...);
char *create_url(vantiq_client_t *vc, const char *rsPath, struct param *params);

vantiq_client_t *vc_from_vme(VME vme);

vantiq_client_t *vc_init(const char *url, const char *authToken, uint8_t apiVersion);
void vc_teardown(vantiq_client_t *vc);

vme_result_t *vc_post(vantiq_client_t *vc, const char *topic, const vmebuf_t *msg, struct param *params);
vme_result_t *vc_put(vantiq_client_t *vc, const char *topic, const vmebuf_t *msg, struct param *params);
vme_result_t *vc_get(vantiq_client_t *vc, const char *rsPath, struct param *params);
vme_result_t *vc_delete(vantiq_client_t *vc, const char *rsPath, struct param *params);
vme_result_t *vc_patch(vantiq_client_t *vc, const char *rsURI, const char *json);
vme_result_t *vc_aggregate(vantiq_client_t *vc, const char *rsURI, struct param *params);
vme_result_t *vc_execute(vantiq_client_t *vc, const char *rsURI, const char *argsDoc);
vme_result_t *vc_query(vantiq_client_t *vc, const char *rsURI, const char *qParams);

#endif
