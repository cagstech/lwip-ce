#ifndef LWIP_CC_H
#define LWIP_CC_H

#undef NDEBUG
#include <debug.h>

#define LWIP_PLATFORM_DIAG(x) do {dbg_printf x;} while(0)

#endif //LWIP_CC_H
