// OSTLERTEKKERS HEN - tiny test helpers shared by all test files.
#pragma once
#include <cstdio>

namespace henu_test {
inline int& passCount() { static int n = 0; return n; }
inline int& failCount() { static int n = 0; return n; }
}  // namespace henu_test

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (cond) { ++henu_test::passCount(); }                                      \
        else { ++henu_test::failCount();                                             \
               std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); }         \
    } while (0)
