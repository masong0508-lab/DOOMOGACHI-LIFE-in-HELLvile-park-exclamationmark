/* test.h - a tiny unit test framework for the native build (owner:
 * foundation). Tests self-register (GCC constructor attribute; tests are
 * host-only), so a new test file just needs TEST() functions:
 *
 *   TEST(rng_is_deterministic) {
 *       CHECK(rng_next(&a) == rng_next(&b));
 *       CHECK_EQ_U(x, 0x1234u);
 *   }
 *
 * Run: $(BUILD)/tests [--golden DIR] [--tmp DIR] [--update-golden] [filter]
 */
#ifndef DOOMO_TEST_H
#define DOOMO_TEST_H

#include <stdio.h>
#include <string.h>
#include "native.h"

typedef void (*TestFn)(void);
void test_register(const char *name, const char *file, TestFn fn);
void test_fail(const char *file, int line, const char *msg);
int  test_failed_now(void);

/* settings from the command line */
extern const char *test_golden_dir;
extern const char *test_tmp_dir;
extern int test_update_golden;

/* Fresh game state: options at defaults, an empty island, quiet input,
 * no toasts, scene stack cleared. */
void test_reset(void);

#define TEST(name)                                                        \
    static void name(void);                                               \
    __attribute__((constructor)) static void reg_##name(void)             \
    { test_register(#name, __FILE__, name); }                             \
    static void name(void)

#define CHECK(cond) do { if (!(cond)) { \
        test_fail(__FILE__, __LINE__, "CHECK(" #cond ")"); return; } } while (0)
#define CHECK_MSG(cond, ...) do { if (!(cond)) { char m_[1024]; \
        snprintf(m_, sizeof(m_), __VA_ARGS__); \
        test_fail(__FILE__, __LINE__, m_); return; } } while (0)
#define CHECK_EQ(a, b) do { long long a_ = (long long)(a), b_ = (long long)(b); \
        if (a_ != b_) { char m_[1024]; \
            snprintf(m_, sizeof(m_), "%s == %s  (%lld != %lld)", #a, #b, a_, b_); \
            test_fail(__FILE__, __LINE__, m_); return; } } while (0)
#define CHECK_EQ_U(a, b) do { unsigned long long a_ = (unsigned long long)(a), \
        b_ = (unsigned long long)(b); \
        if (a_ != b_) { char m_[1024]; \
            snprintf(m_, sizeof(m_), "%s == %s  (0x%llX != 0x%llX)", #a, #b, a_, b_); \
            test_fail(__FILE__, __LINE__, m_); return; } } while (0)
#define CHECK_STR(a, b) do { const char *a_ = (a), *b_ = (b); \
        if (strcmp(a_, b_) != 0) { char m_[300]; \
            snprintf(m_, sizeof(m_), "%s == %s  (\"%s\" != \"%s\")", #a, #b, a_, b_); \
            test_fail(__FILE__, __LINE__, m_); return; } } while (0)

#endif
