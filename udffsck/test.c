
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
/* A test case that does nothing and succeeds. */

#define DEMO 1

#if DEMO
// Mock returning always 1
int always_one(int value) {
    return 1;
}

static void null_test_success(void **state) {
    (void) state; /* unused */
    assert_int_equal(always_one(1), 1);
}

static void null_test_fail(void **state) {
    (void) state; /* unused */
    assert_int_equal(always_one(0), 0);   
}
#endif



int main(void) {
    const struct CMUnitTest tests[] = {
#if DEMO
        cmocka_unit_test(null_test_success),
        cmocka_unit_test(null_test_fail),
#endif
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
