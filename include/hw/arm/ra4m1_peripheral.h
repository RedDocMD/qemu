#ifndef RA4M1_PERIPHERAL_H
#define RA4M1_PERIPHERAL_H

#include "qemu/osdep.h"
#include "hw/arm/armv7m.h"
#include "hw/arm/ra4m1_regs.h"
#include "hw/char/renesas_sci.h"

#define TYPE_RA4M1_PERIPHERAL "ra4m1-peripheral"
OBJECT_DECLARE_SIMPLE_TYPE(RA4M1PeripheralState, RA4M1_PERIPHERAL)

#define RA4M1_PERIPHERAL_SIZE 0x100000
#define RA4M1_SCI_BASE 0x40070000
#define RA4M1_SCI_OFF 0x20
// FIXME: Don't define this here, pass it instead
#define RA4M1_CPU_HZ (48 << 20)

typedef struct RA4M1PeripheralState {
    SysBusDevice parent_obj;

    RA4M1RegsState regs;
    RA4M1FlashRegsState flash_regs;
    RSCIState sci[10];
} RA4M1PeripheralState;

#endif