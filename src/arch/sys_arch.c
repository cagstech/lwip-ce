#include "lwip/arch.h"
#include <time.h>

u32_t
sys_jiffies(void)
{
    return clock();
}

u32_t
sys_now(void)
{
    return sys_jiffies() * 1000 / CLOCKS_PER_SEC;
}

void
sys_init(void)
{
}
