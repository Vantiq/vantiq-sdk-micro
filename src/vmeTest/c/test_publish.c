#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>

#include "CUnit/Basic.h"
#include "vme.h"
#include "cjson.h"
#include "vme_test.h"

void test_publish()
{
    vmeconfig_t config;
    vme_parse_config("config.properties", &config);
    VME vme = vme_init(config.vantiq_url, config.vantiq_token, 1);


    int fd = open("discovery.json", O_RDONLY);
    CU_ASSERT_TRUE_FATAL(fd > 0);

    vmebuf_t *fullMsg = buf_alloc();
    char buf[256];
    size_t nbytes = 0;
    /* read discovery file */
    while ((nbytes = read(fd, buf, sizeof(buf))) > 0) {
        vmebuf_concat(fullMsg, buf, nbytes);
    }
    close(fd);

    /* send the event */
    {
        vme_result_t *result = vme_publish(vme, "/ChinaUnicom/Smarthome/Discovery", fullMsg->data, fullMsg->len);
        CU_ASSERT_PTR_NULL(result->vme_error_msg);
        vme_free_result(result);
    }

    vme_teardown(vme);
}
