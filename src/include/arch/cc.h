#ifndef LWIP_CC_H
#define LWIP_CC_H

#undef NDEBUG
#include <string.h>
#include <sys/lcd.h>
#include <stdio.h>
#include <debug.h>
#include <graphx.h>

#define LWIP_PLATFORM_DIAG(x) \
  do                          \
  {                           \
    printf x;                 \
    putchar("\n");            \
  } while (0)

#endif // LWIP_CC_H
