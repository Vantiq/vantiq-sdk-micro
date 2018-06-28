#include "CUnit/Basic.h"
#include "gradle_cunit_register.h"
#include "vme_test.h"

int suite_init(void) {
    CU_basic_set_mode(CU_BRM_VERBOSE);
    return 0;
}

int suite_clean(void) {
    return 0;
}

void gradle_cunit_register() {
    CU_pSuite pSuiteVME = CU_add_suite("vme tests", suite_init, suite_clean);
    CU_add_test(pSuiteVME, "test_inserts", test_inserts);
    CU_add_test(pSuiteVME, "test_aggregates", test_aggregates);
    CU_add_test(pSuiteVME, "test_updates", test_updates);
    CU_add_test(pSuiteVME, "test_selects", test_selects);
    CU_add_test(pSuiteVME, "test_publish", test_publish);
    CU_add_test(pSuiteVME, "test_patch", test_patch);
    CU_add_test(pSuiteVME, "test_patch", test_execute);
    CU_add_test(pSuiteVME, "test_deletes", test_deletes);
}
