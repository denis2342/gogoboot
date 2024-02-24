#ifndef __ECB_ECB_DOT_H__
#define __ECB_ECB_DOT_H__

#include <types.h>

#define KISS68030_ROM_BASE              0xFFF00000 /* 512KB */
#define KISS68030_MEM_BASE              0xFFF80000 /* 256KB */
#define KISS68030_SRAM_BASE             0xFFFE0000 /* 64KB (32KB chip is mapped twice) */
#define KISS68030_IO_BASE               0xFFFF0000 /* 64KB */
#define KISS68030_ROM_SIZE              (512*1024)

#define MINI68K_ROM_BASE                0x380000 /* 448KB */
#define MINI68K_ECBMEM_BASE             0x300000 /* 256KB */
#define MINI68K_IO_BASE                 0x3F0000 /* 64KB */
#define MINI68K_ROM_SIZE                (448*1024)

#ifdef TARGET_KISS
#define ECB_ROM_BASE KISS68030_ROM_BASE
#define ECB_ROM_SIZE KISS68030_ROM_SIZE
#define ECB_MEM_BASE KISS68030_ECBMEM_BASE
#define ECB_IO_BASE  KISS68030_IO_BASE
#endif

#ifdef TARGET_MINI
#define ECB_ROM_BASE MINI68K_ROM_BASE
#define ECB_ROM_SIZE MINI68K_ROM_SIZE
#define ECB_MEM_BASE MINI68K_ECBMEM_BASE
#define ECB_IO_BASE  MINI68K_IO_BASE
#endif

/* USB-FIFO card */
#define USB_FIFO_ADDR   0x0C

/* USB-FIFO register */
#define USB_FIFO_IO     0x00
#define USB_FIFO_STATUS 0x01
#define USB_FIFO_FLUSH  0x02
#define USB_FIFO_JUNK   0x03

/* interrupts */
#define MFPIC_I8042_IRQ             1       /* Fixed IRQ but requires a jumper on card to enable */
#define MFPIC_IDE_IRQ               9       /* NOTE! this requires MF/PIC board modification! */
#define MFPIC_TIMERH_IRQ            13      /* configurable 0...15 */
#define MFPIC_TIMERL_IRQ            14      /* configurable 0...15 */
#define MFPIC_UART_IRQ              12      /* assumes MFPIC OPT16/8 bit = 1 */
#define MFPIC_UART_CLK              1843200 /* Hz */

/* MF/PIC card */
#define MFPIC_ADDR            0x40
#define MFPIC_NS32202_OFFSET  0
#define MFPIC_CFGREG_OFFSET   2
#define MFPIC_RTC_OFFSET      3
#define MFPIC_8255_OFFSET     4
#define MFPIC_UART_OFFSET     8

#define MFPIC_NS32202         (MFPIC_ADDR + MFPIC_NS32202_OFFSET)
#define MFPIC_CFGREG          (MFPIC_ADDR + MFPIC_CFGREG_OFFSET)
#define MFPIC_RTC             (MFPIC_ADDR + MFPIC_RTC_OFFSET)
#define MFPIC_8255            (MFPIC_ADDR + MFPIC_8255_OFFSET)
#define MFPIC_UART            (MFPIC_ADDR + MFPIC_UART_OFFSET)

/* bits in KISS68030_MFPIC_RTC register */
#define MFPIC_DS1302_DATA_BIT       1
#define MFPIC_DS1302_WREN_BIT       2 /* Controls data direction of DATA pin */
#define MFPIC_DS1302_CLK_BIT        4
#define MFPIC_DS1302_RESET_BIT      8 /* Chip enable -- active high */

/* MF/PIC IDE via 8255 */
#define PPIDE_LSB                       0 /* 8255 port A */
#define PPIDE_MSB                       1 /* 8255 port B */
#define PPIDE_SIGNALS                   2 /* 8255 port C */
#define PPIDE_CONTROL                   3 /* 8255 command register */

/* NS32202 registers */
#define NS32202_HVCT                    0  /* 8-bit byte size */
#define NS32202_SVCT                    1  /* 8-bit byte size */
#define NS32202_ELTG                    2  /* 16-bit word size */
#define NS32202_TPL                     4  /* 16-bit word size */
#define NS32202_IPND                    6  /* 16-bit word size */
#define NS32202_ISRV                    8  /* 16-bit word size */
#define NS32202_IMSK                    10 /* 16-bit word size */
#define NS32202_CSRC                    12 /* 16-bit word size */
#define NS32202_FPRT                    14 /* 16-bit word size */
#define NS32202_MCTL                    16 /* 8-bit byte size */
#define NS32202_OCASN                   17 /* 8-bit byte size */
#define NS32202_CIPTR                   18 /* 16-bit word size */
#define NS32202_PDAT                    19 /* 8-bit byte size */
#define NS32202_IPS                     20 /* 8-bit byte size */
#define NS32202_PDIR                    21 /* 8-bit byte size */
#define NS32202_CCTL                    22 /* 8-bit byte size */
#define NS32202_CICTL                   23 /* 8-bit byte size */
#define NS32202_LCSV                    24 /* 16-bit word size */
#define NS32202_HCSV                    26 /* 16-bit word size */
#define NS32202_LCCV                    28 /* 16-bit word size */
#define NS32202_HCCV                    30 /* 16-bit word size */
#define NS32202_SVCTI                   32 /* 8-bit byte size */
#define NS32202_EOI                     32 /* 8-bit byte size */
#define NS32202_CLK_INPUT MFPIC_UART_CLK   /* shares the UART crystal */

static volatile uint8_t * const ECB_DEVICE_IO = (volatile uint8_t *)ECB_IO_BASE;
static volatile uint8_t * const ECB_PAUSE = (volatile uint8_t *)ECB_ROM_BASE;

static inline uint8_t ecb_read_byte(uint16_t addr)
{
    return ECB_DEVICE_IO[addr];
}

static inline void ecb_write_byte(uint16_t addr, uint8_t val)
{
    ECB_DEVICE_IO[addr] = val;
}

static inline void ecb_write_byte_pause(uint16_t addr, uint8_t val)
{
    ECB_DEVICE_IO[addr] = val;
    *ECB_PAUSE = 0x00;
}

static inline void ecb_slow_down(void)
{
    *ECB_PAUSE = 0x00;
}

#endif
