#ifndef RA4M1_REGS_H
#define RA4M1_REGS_H

#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/sysbus.h"
#include "chardev/char-fe.h"

#define TYPE_RA4M1_REGS "ra4m1-regs"
OBJECT_DECLARE_SIMPLE_TYPE(RA4M1RegsState, RA4M1_REGS)

struct __region {
    hwaddr off;
    hwaddr size;
    hwaddr shift;
};

#define RA4M1_REG_REGION_CNT 5
extern struct __region regions[RA4M1_REG_REGION_CNT];

#define PCNTR_CNT 10
#define GPT_CNT 8

struct __attribute__((packed, aligned(32))) pcntr {
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

struct __attribute__((packed, aligned(32))) gpt_regs {
    uint32_t gtwp;
    uint32_t gtstr;
    uint32_t gtstp;
    uint32_t gtclr;
    uint32_t gtssr;
    uint32_t gtpsr;
    uint32_t gtcsr;
    uint32_t gtupsr;
    uint32_t gtdnsr;
    uint32_t gticasr;
    uint32_t gticbsr;
    uint32_t gtcr;
    uint32_t gtuddtyc;
    uint32_t gtior;
    uint32_t gtintad;
    uint32_t gtst;
    uint32_t gtber;
    uint32_t gtcnt;
    uint32_t gtccra;
    uint32_t gtccrb;
    uint32_t gtccrc;
    uint32_t gtccre;
    uint32_t gtccrd;
    uint32_t gtccrf;
    uint32_t gtpr;
    uint32_t gtpbr;
    uint32_t gtdtcr;
    uint32_t gtdvu;
};

#define PIN_CNT 27
#define GPT_CNT 8
#define PORT_CNT 10
#define PORT_PIN_CNT 16
#define VREF 5

typedef struct RA4M1RegsState {
    SysBusDevice parent_obj;
    MemoryRegion mmio[RA4M1_REG_REGION_CNT];
    CharBackend chr_ws;
    CharBackend chr_sock;

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
    bool analog_enabled[PIN_CNT];
    bool gpt_on[GPT_CNT];
    struct gpt_regs gpt_regs[GPT_CNT];
    uint32_t pmnpfs[PORT_CNT][PORT_PIN_CNT];
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
