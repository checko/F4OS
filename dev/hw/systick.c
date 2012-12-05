#include <stdint.h>
#include <dev/registers.h>
#include <dev/hw/systick.h>

void init_systick(void) {
    /* 250us  at 168Mhz */
    *SYSTICK_RELOAD = 42000;
    *SYSTICK_VAL = 0;
    *SYSTICK_CTL = 0x00000007;

    /* Set PendSV to lowest priority */
    *NVIC_IPR14 = 0xFF;
}

