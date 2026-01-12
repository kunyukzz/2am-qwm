/**
 * My own build system: 2AM Builder
 * Original source: https://github.com/kunyukzz/2am-builder.git
 */

#define TWO_AM_BUILD_IMPL
#include "2am-builder.h"

int main(void)
{
    AM_INIT();
    AM_SET_COMPILER("clang", "c99");
    AM_SET_COMPILER_WARN("-Wall, -Wextra");
    AM_SET_FLAGS("-O2");
    AM_SET_TARGET_NAME("qwm");

    AM_SET_SOURCE_ALL("src");
    AM_SET_TARGET_DIRS("bin", "build");

    AM_USE_LIB("xcb");

    AM_BUILD(BUILD_EXE, true);
    AM_RESET();

    return 0;
}

