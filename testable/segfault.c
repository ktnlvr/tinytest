#include "tinytest.h"

TINYTEST_TEST(segfault) {
    ((void(*)(void))(0))();
}
