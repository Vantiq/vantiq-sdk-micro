#include "vme.h"
#include "cjson.h"

void test_selects(void);
void test_inserts(void);
void test_deletes(void);
void test_updates(void);
void test_aggregates(void);
void test_publish(void);
void test_patch(void);
void test_execute(void);

char *find_instance_id(vme_result_t *result);
cJSON *find_instance_prop(cJSON *instance, const char *propName);
cJSON *remove_prop(cJSON *instance, const char *propName);
