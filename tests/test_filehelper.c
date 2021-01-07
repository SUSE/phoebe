#include "test.h"

#include <errno.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "filehelper.h"

struct test_state {
    FILE *pFile;
    char file_name[18];
    tuning_params_t *parameters;
};

static int setup(void **state) {
    static const char TEMP_FILE_TEMPLATE[18] = "/tmp/phoebeXXXXXX";
    struct test_state *initial_state = calloc(1, sizeof(struct test_state));
    if (initial_state == NULL) {
        return -1;
    }
    memcpy(initial_state->file_name, TEMP_FILE_TEMPLATE,
           sizeof(TEMP_FILE_TEMPLATE));

    const int fd = mkstemp(initial_state->file_name);
    if (fd == -1) {
        fail_msg("Could not create a temporary file, got %s", strerror(errno));
    }
    close(fd);
    initial_state->pFile = fopen(initial_state->file_name, "w");

    *state = initial_state;

    return 0;
}

static int teardown(void **state) {
    struct test_state *to_destroy = *(struct test_state **)state;
    if (to_destroy->parameters != NULL) {
        free(to_destroy->parameters);
    }
    const int unlink_res = unlink(to_destroy->file_name);
    free(to_destroy);
    return unlink_res;
}

static int use_real_fgetc = 1;
static int use_real_feof = 1;

int __real_fgetc(FILE *stream);
int __wrap_fgetc(FILE *stream) {
    if (use_real_fgetc) {
        return __real_fgetc(stream);
    }
    function_called();
    check_expected_ptr(stream);

    return mock();
}

int __real_feof(FILE *stream);
int __wrap_feof(FILE *stream) {
    if (use_real_feof) {
        return __real_feof(stream);
    }
    function_called();
    check_expected_ptr(stream);
    return mock();
}

void allocateMemoryReturnsRetFailWhenFileNULL() {
    assert_int_equal(RET_FAIL, allocateMemoryBasedOnInputAndMaxLearningValues(
                                   NULL, NULL, NULL));
}

void allocateMemoryReturnsRetFailWhenFileHasLessThanOneLine() {
    int dummy = 0;
    FILE *pFile = (FILE *)&dummy;

    use_real_fgetc = 0;
    use_real_feof = 0;
    will_return(__wrap_feof, 1);
    expect_memory(__wrap_feof, stream, &dummy, sizeof(dummy));
    expect_function_call(__wrap_feof);

    assert_int_equal(RET_FAIL, allocateMemoryBasedOnInputAndMaxLearningValues(
                                   pFile, NULL, NULL));
}

void allocateMemoryInitializesReferenceValues(void **state) {
    struct test_state *temp = *(struct test_state **)state;

    fprintf(temp->pFile, "This is a header\n1,1,1,1,0.001,64,64,1,1,\n");
    fclose(temp->pFile);

    FILE *pFile = fopen(temp->file_name, "r");

    use_real_fgetc = 1;
    use_real_feof = 1;

    const app_settings_t app_settings = {.max_learning_values = 16};
    all_values_t reference_values = {};

    assert_int_equal(1, allocateMemoryBasedOnInputAndMaxLearningValues(
                            pFile, &app_settings, &reference_values));
    assert_int_equal(reference_values.totalLength,
                     app_settings.max_learning_values + 1);
    assert_int_equal(reference_values.validValues, 0);

    temp->parameters = reference_values.parameters;
}

void allocateMemoryInitializesReferenceValuesForSuperLongLines(void **state) {
    struct test_state *tmp = *(struct test_state **)state;

    static const char S[4] = "bar";
    fprintf(tmp->pFile, "header\n");
    for (int i = 0; i < 1024 * 1024; ++i) {
        assert_int_equal(3, fwrite(S, 1, 3, tmp->pFile));
    }
    fprintf(tmp->pFile, "\n");

    fclose(tmp->pFile);

    FILE *pFile = fopen(tmp->file_name, "r");

    use_real_fgetc = 1;
    use_real_feof = 1;

    app_settings_t app_settings = {.max_learning_values = 16};
    all_values_t reference_values = {};

    assert_int_equal(1, allocateMemoryBasedOnInputAndMaxLearningValues(
                            pFile, &app_settings, &reference_values));
    assert_int_equal(reference_values.totalLength,
                     app_settings.max_learning_values + 1);
    assert_int_equal(reference_values.validValues, 0);

    tmp->parameters = reference_values.parameters;
}

extern int runFileHelperTests() {
    const struct CMUnitTest allocMemTests[] = {
        cmocka_unit_test(allocateMemoryReturnsRetFailWhenFileNULL),
        cmocka_unit_test(
            allocateMemoryReturnsRetFailWhenFileHasLessThanOneLine),
        cmocka_unit_test_setup_teardown(
            allocateMemoryInitializesReferenceValues, setup, teardown),
        cmocka_unit_test_setup_teardown(
            allocateMemoryInitializesReferenceValuesForSuperLongLines, setup,
            teardown)};

    return cmocka_run_group_tests_name(
        "allocateMemoryBasedOnInputAndMaxLearningValues tests", allocMemTests,
        NULL, NULL);
}
