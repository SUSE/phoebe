# How to write tests

Phoeβe is using the [cmocka](https://cmocka.org/) test framework for unit
testing. Tests *should* be added for all new functions, especially for low level
functions that include system calls, as cmocka allows to mock these calls and
thus failures can and **should** be checked as well.


## Adding a simple unit test

1. Create a new C file in the `tests` subdirectory (we will call this file
   `test_foo.c` for now), with the following contents:
```C
#include "test.h"

void aSimpleTest() {
}

extern int runFooTests() {
  const struct CMUnitTest fooTests[] = {
    cmocka_unit_test(aSimpleTest)};

  return cmocka_run_group_tests_name(
    "fooTests", fooTests);
}
```
   and add the file to the list of unit tests in `tests/meson.build`.

2. Implement some tests using [cmocka's API](https://api.cmocka.org/).


## Mocking system calls

If your function performs system calls then you might have to mock these to be
able to simulate failure conditions.

We follow the approach that has been outlined in this [blog
post](https://blog.microjoe.org/2017/unit-tests-c-cmocka-coverage-cmake.html#system-call-mocking-with-wrap)
and utilize the linker's `--wrap` flag, that can replace external functions.

You will have to add the respective system call to the `link_args` of the
`unit_tests` executable in `tests/meson.build` and implement the `__wrap_foo`
function (assuming that you wrapped `foo`). **Please** ensure that by default
the real system call is used (it is available via `__real_foo()`), e.g. as
follows:
```c
static int use_real_foo = 1;

int __real_foo();
int __wrap_foo() {
    if (use_real_foo) {
        return __real_foo();
    }
    /* setup mocks or expectations */
    return mock();
}
```
