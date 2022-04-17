#include "tinytest.h"

#include <stdlib.h>
#include <signal.h>

TINYTEST_TEST(illegal_instruction) {
    raise(SIGILL);
}
