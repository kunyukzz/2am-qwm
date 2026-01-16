/**
 * My own build system: 2AM Builder
 * Original source: https://github.com/kunyukzz/2am-builder.git
 */

#define TWO_AM_BUILD_IMPL
#include "2am-builder.h"

static void set_target(const char *name, const char *bin_loc,
                       const char *obj_loc)
{
    AM_SET_TARGET_NAME(name);
    AM_SET_TARGET_DIRS(bin_loc, obj_loc);
}

int main(void)
{
    AM_INIT();
    AM_SET_COMPILER("clang", "c99");
    AM_SET_COMPILER_WARN("-Wall, -Wextra");
    AM_SET_FLAGS("-O2");
    AM_SET_SOURCE_ALL("src");

    set_target("qwm", "bin", "build");

    // this for testing on my own hardware
    // set_target("qwm-test", "bin-test", "build-test");

    AM_USE_LIB("xcb");

    AM_BUILD(BUILD_EXE, true);
    AM_RESET();

    return 0;
}

