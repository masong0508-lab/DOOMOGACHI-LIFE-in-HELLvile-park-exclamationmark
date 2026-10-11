/* test_main.c - runner for the native unit tests (owner: foundation). */
#include <stdlib.h>
#include <sys/stat.h>
#include "test.h"

#define MAX_TESTS 512

typedef struct { const char *name, *file; TestFn fn; } TestEntry;
static TestEntry tests[MAX_TESTS];
static int ntests, cur_failed, total_failed_checks;

const char *test_golden_dir = "core/tests/golden";
const char *test_tmp_dir = "/tmp/doomo-tests";
int test_update_golden;

void test_register(const char *name, const char *file, TestFn fn)
{
    if (ntests < MAX_TESTS) {
        tests[ntests].name = name;
        tests[ntests].file = file;
        tests[ntests].fn = fn;
        ntests++;
    }
}

void test_fail(const char *file, int line, const char *msg)
{
    const char *base = strrchr(file, '/');
    fprintf(stderr, "    FAIL %s:%d: %s\n", base ? base + 1 : file, line, msg);
    cur_failed = 1;
    total_failed_checks++;
}

int test_failed_now(void) { return cur_failed; }

void test_reset(void)
{
    while (scene_depth() > 0) scene_pop();
    input_reset();
    ui_toast_clear();
    memset(&doomo_town, 0, sizeof(doomo_town));
    opt_defaults();
    town_new(TOWN_FREEPLAY, 1234, TOWN_POP_EMPTY);
    rng_seed(&doomo_rng, 99);
}

static int cmp_name(const void *a, const void *b)
{
    const TestEntry *x = (const TestEntry *)a, *y = (const TestEntry *)b;
    int c = strcmp(x->file, y->file);
    return c ? c : strcmp(x->name, y->name);
}

int main(int argc, char **argv)
{
    NatOptions o;
    const char *filter = NULL;
    int i, passed = 0, failed = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--golden") == 0 && i + 1 < argc) test_golden_dir = argv[++i];
        else if (strcmp(argv[i], "--tmp") == 0 && i + 1 < argc) test_tmp_dir = argv[++i];
        else if (strcmp(argv[i], "--update-golden") == 0) test_update_golden = 1;
        else filter = argv[i];
    }
    mkdir(test_tmp_dir, 0777);
    memset(&o, 0, sizeof(o));
    o.w = 240; o.h = 160; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
    nat_init(&o);
    doomo_init();                       /* palette, art, defaults */
    qsort(tests, (size_t)ntests, sizeof(tests[0]), cmp_name);
    for (i = 0; i < ntests; i++) {
        if (filter && !strstr(tests[i].name, filter) && !strstr(tests[i].file, filter)) continue;
        cur_failed = 0;
        test_reset();
        tests[i].fn();
        if (cur_failed) { failed++; fprintf(stderr, "FAIL  %s\n", tests[i].name); }
        else { passed++; printf("ok    %s\n", tests[i].name); }
    }
    printf("\n%d passed, %d failed (%d failed checks)\n", passed, failed, total_failed_checks);
    return failed ? 1 : 0;
}
