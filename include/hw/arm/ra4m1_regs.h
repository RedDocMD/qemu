#ifndef RA4M1_REGS_H
#define RA4M1_REGS_H

#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/sysbus.h"

#define TYPE_RA4M1_REGS "ra4m1-regs"
OBJECT_DECLARE_SIMPLE_TYPE(RA4M1RegsState, RA4M1_REGS)

#define RA4M1_REGS_LO_OFF 0x40000000
#define RA4M1_REGS_LO_SIZE 0x70000
#define RA4M1_REGS_HI_OFF 0x40080000
#define RA4M1_REGS_HI_SIZE 0x80000
#define RA4M1_REGS_HI_SHIFT 0x80000

struct __region {
    hwaddr off;
    hwaddr size;
    hwaddr shift;
};

#define RA4M1_REG_REGION_CNT 2
extern struct __region regions[RA4M1_REG_REGION_CNT];

#define PCNTR_CNT 10

struct __attribute__((packed)) pcntr {
    union {
        uint32_t dw;
        struct {
            uint16_t pdr;
            uint16_t podr;
        } hw;
    } pcntr1;

    union {
        uint32_t dw;
        struct {
            uint16_t pidr;
            uint16_t eidr;
        } hw;
    } pcntr2;

    union {
        uint32_t dw;
        struct {
            uint16_t posr;
            uint16_t porr;
        } hw;
    } pcntr3;
};

typedef struct RA4M1RegsState {
    SysBusDevice parent_obj;
    MemoryRegion mmio[RA4M1_REG_REGION_CNT];

    uint8_t vbtcr1;
    uint8_t vbtsr;
    uint16_t prcr;
    uint16_t fcachee;
    uint32_t sckdivcr;
    uint8_t sckscr;
    uint8_t momcr;
    uint8_t moscwtcr;
    uint8_t sosccr;
    uint8_t somcr;
    uint8_t opccr;
    uint8_t hococr;
    uint8_t oscsf;
    uint8_t memwait;
    uint16_t usbfs_syscfg;
    struct pcntr pcntr[PCNTR_CNT];
} RA4M1RegsState;

#define RA4M1_FLASH_REGS_OFF 0x407E0000
#define RA4M1_FLASH_REGS_SIZE 0x10000

#define TYPE_RA4M1_FLASH_REGS "ra4m1-flash-regs"
OBJECT_DECLARE_SIMPLE_TYPE(RA4M1FlashRegsState, RA4M1_FLASH_REGS)

typedef struct RA4M1FlashRegsState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
} RA4M1FlashRegsState;

#endif