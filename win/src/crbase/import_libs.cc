#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_COMPILER_MSVC)
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "wbemuuid.lib") // for wmi
#endif
