#include <stdlib.h>
#include <stdbool.h>
#include "q40hw.h"
#include "q40ide.h"
#include "q40isa.h"
#include "q40uart.h"

extern const char copyright_msg[];

/* TODO:
 * DONE - 040 cache modes
 * DONE - debug odd crashes (SOFTROM bug)
 * DONE - printf
 * DONE - memcpy
 * DONE - video mode?
 * DONE - blank the screen
 * DONE - measure installed RAM
 * - 68K exception handler
 * - ISA bus reset
 * - some sort of timer (add interrupt support, then use the timer tick?)
 * - configure the other master chip's registers -- interrupt control?
 * - IDE interface + FAT filesystem
 * - SOFTROM feature clone, so we can test new ROMs
 * - linux loader
 * - NE2000 driver
 * - ultimately target a port back to kiss-68030?
 */

void boot_q40(void)
{
    q40_led(false);

    /* TODO: ISA bus reset ... requires some sort of delay function I think */

    uart_init();
    uart_write_str(copyright_msg);

    q40_measure_ram_size();
    printf("RAM installed: %d MB\n", ram_size>>20);

    printf("Initialise video: ");
    q40_graphics_init(3);
    printf("done\n");

    printf("Initialise IDE: ");
    q40_ide_init();
    printf("done\n");

    q40_led(true);
}
