#ifndef RA4M1_SCI_H
#define RA4M1_SCI_H

#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/sysbus.h"
#include "chardev/char-fe.h"

#define TYPE_RA4M1_SCI "ra4m1-sci"
OBJECT_DECLARE_SIMPLE_TYPE(RA4M1SciState, RA4M1_SCI)

typedef struct RA4M1SciState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    CharBackend chr;

    struct {
        struct {
            unsigned int cks : 2, mp : 1, stop : 1, pm : 1, pe : 1, chr : 1,
                cm : 1;
        } smr;
        uint8_t brr;
        struct {
            unsigned int cke : 2, teie : 1, mpie : 1, re : 1, te : 1, rie : 1,
                tie : 1;
        } scr;
        uint8_t tdr;
        struct {
            unsigned int mpbt : 1, mpb : 1, tend : 1, per : 1, fer : 1,
                orer : 1, rdrf : 1, tdre : 1;
        } ssr;
        uint8_t rdr;
        struct {
            unsigned int smif : 1, : 1, sinv : 1, sdir : 1, chr1 : 1, : 2,
                bcp2 : 1;
        } scmr;
        struct {
            unsigned int : 2, brme : 1, abcse : 1, abcs : 1, nfen : 1, bgdm : 1,
                rxdesel : 1;
        } semr;
    } regs;
} RA4M1SciState;

#endif