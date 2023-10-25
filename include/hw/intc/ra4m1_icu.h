#ifndef RA4M1_ICU_H
#define RA4M1_ICU_H

#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/sysbus.h"

#define RA4M1_ICU_BASE 0x40006000
#define RA4M1_ICU_SIZE 0x1000

#define TYPE_RA4M1_ICU "ra4m1-icu"
OBJECT_DECLARE_SIMPLE_TYPE(RA4M1IcuState, RA4M1_ICU)

typedef struct RA4M1IcuState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;

    uint32_t ielsr[32];
} RA4M1IcuState;

#endif