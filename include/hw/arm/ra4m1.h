#ifndef RA4M1_H
#define RA4M1_H

#include "qemu/osdep.h"
#include "hw/arm/armv7m.h"
#include "hw/arm/ra4m1_regs.h"
#include "hw/char/renesas_sci.h"

#define RA4M1_CPU_NAME ARM_CPU_TYPE_NAME("cortex-m4")
#define RA4M1_CORE_COUNT 1
#define RA4M1_PERIPHERAL_BASE 0x40000000
#define RA4M1_SCI2_BASE 0x40070040
#define RA4M1_SRAM_SIZE (32 << 10)
#define RA4M1_SRAM_BASE 0x20000000
#define RA4M1_FLASH_BASE 0
#define RA4M1_FLASH_SIZE (256 << 10)
#define RA4M1_ONCHIP_FLASH_BASE 0x407FB19C
#define RA4M1_ONCHIP_FLASH_SIZE 4
#define RA4M1_CPU_HZ (48 << 20)
#define RA4M1_NUM_IRQ 32
#define RA4M1_VT_SIZE (RA4M1_NUM_IRQ + 16)
#define DEFAULT_STACK_SIZE (1 << 10)
#define RA4M1_SCI_BASE 0x40070000
#define RA4M1_SCI_OFF 0x20

#define TYPE_RA4M1 "ra4m1"
OBJECT_DECLARE_TYPE(RA4M1State, RA4M1Class, RA4M1)

typedef struct RA4M1State {
    DeviceState parent_state;
    ARMv7MState armv7m;
    Clock *sysclk;
    MemoryRegion sram;
    MemoryRegion flash;
    MemoryRegion onchip_flash;
    RA4M1RegsState regs;
    RA4M1FlashRegsState flash_regs;
    RSCIState sci[10];
} RA4M1State;

typedef struct RA4M1Class {
    DeviceClass parent_class;
} RA4M1Class;

#endif