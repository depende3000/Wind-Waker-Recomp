// tww_sdk_smoke: a headless test program for tww_sdk (no window, no GPU).
//
// Each test file in native/sdk/tests registers its tests by name, so the phase 2 steps add test
// files without editing a shared one:
//
//   TWW_SMOKE_TEST(threads) {
//       TWW_SMOKE_CHECK(OSIsThreadTerminated(&t));
//       return true;
//   }
//
// `tww_sdk_smoke` runs every test; `tww_sdk_smoke <name>...` runs the named ones; `--list` lists
// them. It prints "ok" and exits 0 when all pass.
#ifndef TWW_SDK_TESTS_SMOKE_H
#define TWW_SDK_TESTS_SMOKE_H

#include <cstdio>

namespace tww_smoke {

using TestFn = bool (*)();

// Adds a test to the registry; used by TWW_SMOKE_TEST at static initialisation.
int Register(const char* name, TestFn fn);

} // namespace tww_smoke

#define TWW_SMOKE_TEST(name)                                                                       \
    static bool tww_smoke_test_##name();                                                           \
    [[maybe_unused]] static const int tww_smoke_reg_##name =                                       \
        ::tww_smoke::Register(#name, &tww_smoke_test_##name);                                      \
    static bool tww_smoke_test_##name()

// Fails the current test with the file, line and expression.
#define TWW_SMOKE_CHECK(expr)                                                                      \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr);          \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#endif // TWW_SDK_TESTS_SMOKE_H
