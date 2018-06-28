/*
 * encapsulate the logic for connecting to the vantiq server
 * and sending discovery messages
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <curl/curl.h>

#include "vantiq_client.h"

#define AUTH_HDR_KEY "Authorization: Bearer "
#define AUTH_URL_PATH "/authenticate"
#define JSON_CONTENTTYPE "Content-Type: application/json"

#define log_null(x)

#define VC_MAGIC 0x07

#define REST_API "/api/v"

vme_result_t *_putorpost(vantiq_client_t *vc, const char *rsURI, const vmebuf_t *msg, struct param *params);

struct param *build_param(struct param *head, const char *key, const char *value)
{
    struct param *p = malloc(sizeof(struct param));
    p->key = strdup(key);
    p->value = strdup(value);
    p->next = head;
    return p;
}

void free_params(struct param *params)
{
    struct param *next;
    while (params != NULL) {
        next = params->next;
        params->next = NULL;
        free(params->key);
        free(params->value);
        free(params);
        params = next;
    }
}

/*
 * set up the authorization token to access VANTIQ cloud service. might be a good idea
 * to obfuscate this string and token value.
 */
char *create_auth_hdr(vantiq_client_t *vc, const char *token) {
    int leadSz = sizeof(AUTH_HDR_KEY);
    char *hdr = malloc(strlen(token) +  leadSz + 1);
    strcpy(hdr, AUTH_HDR_KEY);
    strcpy(hdr+leadSz-1, token);
    return hdr;
}

#define COUNT_HEADER "X-Total-Count:"
static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    vantiq_client_t *vc = (vantiq_client_t *)userdata;
    size_t len = nitems * size;
    size_t headerNameSz = sizeof(COUNT_HEADER)-1;
    /* received header is nitems * size long in 'buffer' NOT ZERO TERMINATED */
    /* 'userdata' is set with CURLOPT_HEADERDATA */
    if (len > headerNameSz && memcmp(buffer, COUNT_HEADER, headerNameSz) == 0) {
        char countBuf[32];
        int pos = (int)headerNameSz;
        memcpy(countBuf, buffer + pos, len - pos);
        countBuf[len-pos] = 0;
        vc->result_count = atoi(countBuf);
    }
    return len;
}

static size_t
write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    vantiq_client_t *vc = (vantiq_client_t *)userp;

    if (vc->recv_callback != NULL) {
        realsize = vc->recv_callback(vc->callback_state, contents, realsize);
    } else {
        vc->recv_buf = vmebuf_ensure_incr_size(vc->recv_buf, realsize);
        vmebuf_concat(vc->recv_buf, contents, realsize);
    }
    return realsize;
}

static size_t read_callback(void *dest, size_t size, size_t nmemb, void *userp)
{
    // todo: data rewind handling?
    vantiq_client_t *vc = (vantiq_client_t *) userp;
    size_t buffer_size = size * nmemb;

    if (vc->send_state.sizeleft) {
        /* copy as much as possible from the source to the destination */
	    size_t copy_this_much = vc->send_state.sizeleft;
	    if (copy_this_much > buffer_size)
            copy_this_much = buffer_size;
        memcpy(dest, vc->send_state.readptr, copy_this_much);

        vc->send_state.readptr += copy_this_much;
        vc->send_state.sizeleft -= copy_this_much;
        return copy_this_much; /* we copied this many bytes */
    }

	return 0; /* no more data left to deliver */
}

char *create_url(vantiq_client_t *vc, const char *rsURI, struct param *params)
{
    size_t len = strlen(vc->server_url) + strlen(rsURI) + 1;
    vmebuf_t *urlbuf = vmebuf_ensure_size(NULL, len);
    
    vmebuf_concat(urlbuf, vc->server_url, strlen(vc->server_url));
    vmebuf_concat(urlbuf, rsURI, strlen(rsURI));
    
    // ok now deal with the URL parameters
    if (params != NULL)
        vmebuf_push(urlbuf, '?');
    while (params != NULL) {
        vmebuf_concat(urlbuf, params->key, strlen(params->key));
        vmebuf_push(urlbuf, '=');
        char *value = curl_easy_escape(vc->curl, params->value, (int)strlen(params->value));
        if (value != NULL) {
            vmebuf_concat(urlbuf, value, strlen(value));
            free(value);
        } else {
            // todo: how to deal with error here? for now use the un-escaped string
            vmebuf_concat(urlbuf, params->value, strlen(params->value));
        }
        if (params->next != NULL) {
            vmebuf_push(urlbuf, ';');
        }
        params = params->next;
    }
    char *strUrl = vmebuf_tostr(urlbuf);
    vmebuf_dealloc(urlbuf);
    return strUrl;
}

void common_curl_setup(vantiq_client_t *vc)
{
    curl_easy_reset(vc->curl);
    /* follow any redirects */
    curl_easy_setopt(vc->curl, CURLOPT_FOLLOWLOCATION, 1L);
    /* SSL Options */
    curl_easy_setopt(vc->curl, CURLOPT_SSL_VERIFYPEER , 1);
    curl_easy_setopt(vc->curl, CURLOPT_SSL_VERIFYHOST , 1);

    /* Provide CA Certs from http://curl.haxx.se/docs/caextract.html */
//    curl_easy_setopt(vc->curl, CURLOPT_CAINFO, "ca-bundle.crt");
    
    /* get verbose debug output please */
    curl_easy_setopt(vc->curl, CURLOPT_VERBOSE, 0L);
    
    /* ask curl to let see the respons body when we get a 400*/
    curl_easy_setopt(vc->curl, CURLOPT_FAILONERROR, 0L);
    
    curl_easy_setopt(vc->curl, CURLOPT_HTTPHEADER, vc->http_hdrs);
    
    /* we want to use our own read function */
    curl_easy_setopt(vc->curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(vc->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(vc->curl, CURLOPT_HEADERFUNCTION, header_callback);
    
    /* pointer to pass to our read function */
    curl_easy_setopt(vc->curl, CURLOPT_READDATA, vc);
    curl_easy_setopt(vc->curl, CURLOPT_WRITEDATA, vc);
    curl_easy_setopt(vc->curl, CURLOPT_HEADERDATA, vc);

    /* sometimes things will hang. don't let that hang the app */
    curl_easy_setopt(vc->curl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(vc->curl, CURLOPT_LOW_SPEED_LIMIT, 30L);
}

/*
 * sanity check that we have a valid client
 */
int vc_is_valid(vantiq_client_t *vc)
{
    return (vc != NULL && vc->magic == VC_MAGIC) ? 1 : 0;
}

/*
 * convert from our VME descriptor to our vantiq client
 */
vantiq_client_t *vc_from_vme(VME vme)
{
    vantiq_client_t *vc = (vantiq_client_t *)vme;
    return vc_is_valid(vc) == 1 ? vc : NULL;
}

/*
 * this function makes a call to the vantiq server with GET /authenticate. if
 * successful, we know things are up and running and communicatiions have been
 * established.
 */
vantiq_client_t *vc_init(const char *url, const char *authToken, uint8_t apiVersion)
{
    vantiq_client_t *vc = malloc(sizeof(vantiq_client_t));
    memset(vc, 0, sizeof(vantiq_client_t));
    vc->magic = VC_MAGIC;
    vc->api_version = apiVersion;
    // we'll always want the authorization header
    vc->http_hdrs = curl_slist_append(NULL, create_auth_hdr(vc, authToken));

    size_t len = strlen(url);
    if (url[len-1] == '/') {
        // trim trailing slash
        vc->server_url = (char *)malloc(len);
        strncpy(vc->server_url, url, len-1);
        vc->server_url[len-1] = '\0';
    } else {
        vc->server_url = strdup(url);
    }
    
    /* results buffer allocated and grown as needed */
    vc->recv_buf = buf_alloc();
    
    /* call is not thread safe -- might need a different place to call this */
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    
    /* Check for errors */
    if (res != CURLE_OK) {
        //sprintf(errorBuf, "initialization of HTTP stack failed: %s", curl_easy_strerror(res));
        return NULL;
    }

    /* get a curl handle */
    vc->curl = curl_easy_init();
    if (vc->curl == NULL) {
        //sprintf(errorBuf, "initialization of HTTP stack failed: %s", curl_easy_strerror(res));
		return NULL;
    }

    // once HEADERS slist is ready, we can call common set up
    common_curl_setup(vc);
    
    len = strlen(vc->server_url) + sizeof(AUTH_URL_PATH);
    /* First authenticate to the vantiq system */
    char *authUrl = malloc(len);
    snprintf(authUrl, len, "%s%s", vc->server_url, AUTH_URL_PATH);
    curl_easy_setopt(vc->curl, CURLOPT_URL, authUrl);
    curl_easy_setopt(vc->curl, CURLOPT_HTTPGET, 1L);


    CURLcode result = curl_easy_perform(vc->curl);

    /* the rest of our operations expect JSON back */
    // TODO: might not want this for every operation
    vc->http_hdrs = curl_slist_append(vc->http_hdrs, JSON_CONTENTTYPE);
    // Disable Expect: 100-continue
    vc->http_hdrs = curl_slist_append(vc->http_hdrs, "Expect:");

    long httpCode = 0;
    curl_easy_getinfo (vc->curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_reset(vc->curl);
    free(authUrl);

    if (result != CURLE_OK && httpCode != 200) {
        //sprintf(errorBuf, "failed to authenticate to VANTIQ");
        return NULL;
    }
    return vc;
}

vme_result_t *prepare_result(CURLcode resCode, const char *protErrMsg, vantiq_client_t *vc)
{
    assert(protErrMsg != NULL);
    assert(vc != NULL);

    vme_result_t *result = (vme_result_t *)malloc(sizeof(vme_result_t));
    memset(result, 0, sizeof(vme_result_t));

    /* Check for errors */
    if (resCode != CURLE_OK) {
        result->vme_error_msg = (strlen(protErrMsg) > 0 ? strdup(protErrMsg) : strdup(curl_easy_strerror(resCode)));
    }

    // we got some kind of response message. could be error explanation or valid results
    if (vc->recv_buf->len > 0) {
        long rc;
        curl_easy_getinfo(vc->curl, CURLINFO_RESPONSE_CODE, &rc);

        result->vme_size = vc->recv_buf->len;

        if (rc >= 400) {
            result->vme_error_msg = malloc(vc->recv_buf->len);
            memcpy(result->vme_error_msg, vc->recv_buf->data, vc->recv_buf->len);
        } else {
            result->vme_json_data = malloc(vc->recv_buf->len);
            memcpy(result->vme_json_data, vc->recv_buf->data, vc->recv_buf->len);
        }
    }
    result->vme_count = vc->result_count;
    vc->result_count = 0;
    return result;
}

vme_result_t *vc_put(vantiq_client_t *vc, const char *rsURI, const vmebuf_t *msg, struct param *params)
{
    common_curl_setup(vc);

    /* Now specify we want to put data */
    curl_easy_setopt(vc->curl, CURLOPT_PUT, 1L);
    return _putorpost(vc, rsURI, msg, params);
}

vme_result_t *vc_post(vantiq_client_t *vc, const char *rsURI, const vmebuf_t *msg, struct param *params)
{
    common_curl_setup(vc);

    /* Now specify we want to post data */
    curl_easy_setopt(vc->curl, CURLOPT_POST, 1L);
    return _putorpost(vc, rsURI, msg, params);
}

/*
 * underpinning for PUT and POST calls. call is responsible for setting the
 * verb prior to calling...
 */
vme_result_t *_putorpost(vantiq_client_t *vc, const char *rsURI, const vmebuf_t *msg, struct param *params)
{
    /* make sure we specify json content type */
    char *topicUrl = create_url(vc, rsURI, params);
    curl_easy_setopt(vc->curl, CURLOPT_URL, topicUrl);

    /* data to send. will actually be sent in the post callback (read_callback) */
    vc->send_state.readptr = msg->data;
    vc->send_state.sizeleft = msg->len;
    // reset our buffer ...
    vmebuf_truncate(vc->recv_buf);

    /*
     If you use POST to a HTTP 1.1 server, you can send data without knowing
     the size before starting the POST if you use chunked encoding. You
     enable this by adding a header like "Transfer-Encoding: chunked" with
     CURLOPT_HTTPHEADER. With HTTP 1.0 or without chunked transfer, you must
     specify the size in the request.
     */
#ifdef USE_CHUNKED
    {
        struct curl_slist *chunk = NULL;
        
        chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
        res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        /* use curl_slist_free_all() after the *perform() call to free this
         list again */
    }
#else
    /* Set the expected POST size. If you want to POST large amounts of data,
     consider CURLOPT_POSTFIELDSIZE_LARGE */
    curl_easy_setopt(vc->curl, CURLOPT_POSTFIELDSIZE, (long)vc->send_state.sizeleft);
#endif

    char errbuf[CURL_ERROR_SIZE];
    errbuf[0] = 0;
    curl_easy_setopt(vc->curl, CURLOPT_ERRORBUFFER, errbuf);
    /* Perform the request, resCode will get the return code */
    CURLcode resCode = curl_easy_perform(vc->curl);

    vme_result_t *result = prepare_result(resCode, errbuf, vc);

    curl_easy_reset(vc->curl);
    free(topicUrl);
    return result;
}

vme_result_t *_getordelete(vantiq_client_t *vc, const char *rsURI, struct param *params)
{
    char *url = create_url(vc, rsURI, params);
    curl_easy_setopt(vc->curl, CURLOPT_URL, url);

    char errBuf[CURL_ERROR_SIZE];
    errBuf[0] = 0;

    vc->recv_buf->len = 0;
    /* Perform the request, res will get the return code */
    CURLcode res = curl_easy_perform(vc->curl);
    vme_result_t *result = prepare_result(res, errBuf, vc);
    curl_easy_reset(vc->curl);
    free(url);

    return result;
}

vme_result_t *vc_get(vantiq_client_t *vc, const char *rsURI, struct param *params)
{
    common_curl_setup(vc);
    curl_easy_setopt(vc->curl, CURLOPT_HTTPGET, 1L);
    return _getordelete(vc, rsURI, params);
}

vme_result_t *vc_delete(vantiq_client_t *vc, const char *rsURI, struct param *params)
{
    common_curl_setup(vc);
    curl_easy_setopt(vc->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    return _getordelete(vc, rsURI, params);
}

vme_result_t *vc_patch(vantiq_client_t *vc, const char *rsURI, const char *json)
{
    common_curl_setup(vc);
    curl_easy_setopt(vc->curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(vc->curl, CURLOPT_POSTFIELDS, json);

    /* make sure we specify json content type */
    char *topicUrl = create_url(vc, rsURI, NULL);
    curl_easy_setopt(vc->curl, CURLOPT_URL, topicUrl);
    vmebuf_truncate(vc->recv_buf);

    char errbuf[CURL_ERROR_SIZE];
    errbuf[0] = 0;
    curl_easy_setopt(vc->curl, CURLOPT_ERRORBUFFER, errbuf);

    /* Perform the request, resCode will get the return code */
    CURLcode resCode = curl_easy_perform(vc->curl);

    vme_result_t *result = prepare_result(resCode, errbuf, vc);

    curl_easy_reset(vc->curl);
    free(topicUrl);
    return result;
}

vme_result_t *vc_aggregate(vantiq_client_t *vc, const char *rsURI, struct param *params)
{
    common_curl_setup(vc);
    curl_easy_setopt(vc->curl, CURLOPT_HTTPGET, 1L);
    vme_result_t *result = _getordelete(vc, rsURI, params);
    return result;
}

vme_result_t *vc_execute(vantiq_client_t *vc, const char *rsURI, const char *argsDoc)
{
    vmebuf_t *msg = vmebuf_ensure_size(NULL, strlen(argsDoc));
    vmebuf_concat(msg, argsDoc, strlen(argsDoc));

    common_curl_setup(vc);
    curl_easy_setopt(vc->curl, CURLOPT_POST, 1L);
    vme_result_t *result = _putorpost(vc, rsURI, msg, NULL);
    vmebuf_dealloc(msg);
    return result;
}

vme_result_t *vc_query(vantiq_client_t *vc, const char *rsURI, const char *qParams)
{
    vmebuf_t *msg = vmebuf_ensure_size(NULL, strlen(qParams));
    vmebuf_concat(msg, qParams, strlen(qParams));
    common_curl_setup(vc);
    curl_easy_setopt(vc->curl, CURLOPT_POST, 1L);
    vme_result_t *result = _putorpost(vc, rsURI, msg, NULL);
    vmebuf_dealloc(msg);
    return result;
}

void vc_teardown(vantiq_client_t *vc)
{
    vc->magic = 0;
    if (vc->recv_buf != NULL) {
        vmebuf_dealloc(vc->recv_buf);
    }
    curl_slist_free_all(vc->http_hdrs);
    curl_easy_cleanup(vc->curl);
    free(vc);
}
