#include "hw/arm/ra4m1_regs.h"
#include "qemu/log.h"
#include "migration/vmstate.h"

#define set_bit_from(dest, src, bit)     \
    do {                                 \
        *(dest) &= ~(1 << (bit));        \
        *(dest) |= (1 << (bit)) & (src); \
    } while (0);

#define set_with_retain(old, new, retain)                               \
    do {                                                                \
        uint64_t val = (new);                                           \
        int bit;                                                        \
        int off = 0, read = 0;                                          \
        while ((sscanf((char *)((uint8_t *)retain + off), "%d%n", &bit, \
                       &read) != EOF)) {                                \
            off += read;                                                \
            set_bit_from(&val, *(old), bit);                            \
        }                                                               \
        *(old) = val;                                                   \
    } while (0);

#define set_with(old, new, with)                                      \
    do {                                                              \
        int bit;                                                      \
        int off = 0, read = 0;                                        \
        while ((sscanf((char *)((uint8_t *)with + off), "%d%n", &bit, \
                       &read) != EOF)) {                              \
            off += read;                                              \
            set_bit_from((old), (new), bit);                          \
        }                                                             \
    } while (0);

#define FIELD_SIZEOF(t, f) (sizeof(((t *)0)->f))

// clang-format off
#define FCACHEE_OFF      0x1C100
#define SCKDIVCR_OFF     0x1E020
#define SCKSCR_OFF       0x1E026
#define MEMWAIT_OFF      0x1E031
#define HOCOCR_OFF       0x1E036
#define OSCSF_OFF        0x1E03C
#define OPCCR_OFF        0x1E0A0
#define MOSCWTCR_OFF     0x1E0A2
#define PRCR_OFF         0x1E3FE
#define MOMCR_OFF        0x1E413
#define VBTCR1_OFF       0x1E41F
#define SOSCCR_OFF       0x1E480
#define SOMCR_OFF        0x1E481
#define VBTSR_OFF        0x1E4B1
#define USBFS_SYSCFG     0x90000

#define PCNTR_BASE       0x40000
#define PCNTR_SHIFT      0x20
// clang-format on

static bool ra4m1_regs_battery_regs_write_allowed(const RA4M1RegsState *s)
{
    return (s->prcr & 0x2);
}

static bool ra4m1_regs_clock_regs_write_allowed(const RA4M1RegsState *s)
{
    return (s->prcr & 0x1);
}

static void ra4m1_regs_reset(DeviceState *dev)
{
    RA4M1RegsState *s = RA4M1_REGS(dev);

    s->vbtcr1 = 0x00;
    s->vbtsr = 0x10;
    s->prcr = 0x0000;
    s->fcachee = 0x0000;
    s->sckdivcr = 0x44044444;
    s->sckscr = 0x01;
    s->momcr = 0x00;
    s->moscwtcr = 0x05;
    s->sosccr = 0x01;
    s->somcr = 0x00;
    s->opccr = 0x02;
    s->hococr = 0x00;
    s->oscsf = 0x01;
    s->memwait = 0x00;
    s->usbfs_syscfg = 0x0000;
    bzero(s->pcntr, sizeof(s->pcntr));
}

static bool is_pcntr_offset(hwaddr off)
{
    if (off < PCNTR_BASE)
        return false;
    return (off - PCNTR_BASE) < FIELD_SIZEOF(RA4M1RegsState, pcntr);
}

static void *pcntr_field(RA4M1RegsState *s, hwaddr addr)
{
    hwaddr off = addr - PCNTR_BASE;
    hwaddr idx_off = off & ~0xF;
    size_t idx = idx_off / PCNTR_SHIFT;
    struct pcntr *pcntr = s->pcntr + idx;

    hwaddr field_off = off & 0xF;
    hwaddr offsets[] = { 0, 2, 4, 6, 8, 12 };

    char *ptr = (char *)pcntr;
    for (int i = 0; i < ARRAY_SIZE(offsets); ++i) {
        if (field_off == offsets[i])
            return ptr + field_off;
    }
    return NULL;
}

static uint64_t read_pcntr(RA4M1RegsState *s, hwaddr addr, unsigned int size)
{
    void *ptr;

    if (!(ptr = pcntr_field(s, addr))) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad read offset 0x%" HWADDR_PRIx " for PCNTR\n",
                      __func__, addr);
        return 0;
    }
    if (size == 2) {
        return *((uint16_t *)ptr);
    } else if (size == 4) {
        return *((uint32_t *)ptr);
    } else {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Invalid read size %d at offset 0x%" HWADDR_PRIx
                      " for PCNTR\n",
                      __func__, size, addr);
        return 0;
    }
}

static void write_pcntr(RA4M1RegsState *s, hwaddr addr, uint64_t val64,
                        unsigned int size)
{
    void *ptr;

    if (!(ptr = pcntr_field(s, addr))) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad read offset 0x%" HWADDR_PRIx " for PCNTR\n",
                      __func__, addr);
        return;
    }
    memcpy(ptr, &val64, size);
}

struct region_idx {
    RA4M1RegsState *state;
    int idx;
};

static uint64_t ra4m1_regs_read(void *opaque, hwaddr addr, unsigned int size)
{
    struct region_idx *reg = opaque;
    RA4M1RegsState *s = reg->state;

    addr += regions[reg->idx].shift;
    // printf("Read Addr = 0x%lx\n", addr);

    if (is_pcntr_offset(addr)) {
        return read_pcntr(s, addr, size);
    }

    switch (addr) {
    case VBTCR1_OFF:
        return s->vbtcr1;
    case VBTSR_OFF:
        return s->vbtsr;
    case PRCR_OFF:
        return s->prcr;
    case FCACHEE_OFF:
        return s->fcachee;
    case SCKDIVCR_OFF:
        return s->sckdivcr;
    case SCKSCR_OFF:
        return s->sckscr;
    case MOMCR_OFF:
        return s->momcr;
    case MOSCWTCR_OFF:
        return s->moscwtcr;
    case SOSCCR_OFF:
        return s->sosccr;
    case SOMCR_OFF:
        return s->somcr;
    case OPCCR_OFF:
        return s->opccr;
    case HOCOCR_OFF:
        return s->hococr;
    case OSCSF_OFF:
        return s->oscsf;
    case MEMWAIT_OFF:
        return s->memwait;
    case USBFS_SYSCFG:
        return s->usbfs_syscfg;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%" HWADDR_PRIx " for regs lo\n",
                      __func__, addr);
        return 0;
    }
}

static void ra4m1_regs_write(void *opaque, hwaddr addr, uint64_t val64,
                             unsigned int size)
{
    struct region_idx *reg = opaque;
    RA4M1RegsState *s = reg->state;

    addr += regions[reg->idx].shift;
    // printf("Write Addr = 0x%lx\n", addr);

    if (is_pcntr_offset(addr)) {
        write_pcntr(s, addr, val64, size);
        return;
    }

    switch (addr) {
    case VBTCR1_OFF:
        if (ra4m1_regs_battery_regs_write_allowed(s)) {
            s->vbtcr1 = (uint8_t)val64;
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "PRCR[1] = 0, can't modify VBTCR1");
        }
        return;
    case VBTSR_OFF:
        if (ra4m1_regs_battery_regs_write_allowed(s)) {
            set_with_retain(&s->vbtsr, (uint8_t)val64, "4");
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "PRCR[1] = 0, can't modify VBTSR");
        }
        return;
    case PRCR_OFF:
        if ((val64 & 0xFF00) != 0xA500) {
            qemu_log_mask(LOG_GUEST_ERROR, "PRCR[15:8] must be A5");
        } else {
            set_with(&s->prcr, (uint16_t)val64, "0 1 3");
        }
        return;
    case FCACHEE_OFF:
        set_with(&s->fcachee, (uint16_t)val64, "0");
        return;
    case SCKDIVCR_OFF:
        if (ra4m1_regs_clock_regs_write_allowed(s)) {
            set_with_retain(&s->sckdivcr, (uint32_t)val64,
                            "3 7 11 15 16 17 18 19 20 21 22 23 27 31");
        } else {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "PRCR[0] = 0, can't modify SCKDIVCR");
        }
        return;
    case SCKSCR_OFF:
        if (ra4m1_regs_clock_regs_write_allowed(s)) {
            set_with(&s->sckscr, (uint8_t)val64, "0 1 2");
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "PRCR[0] = 0, can't modify SCKSCR");
        }
        return;
    case MOMCR_OFF:
        if (ra4m1_regs_clock_regs_write_allowed(s)) {
            set_with(&s->momcr, (uint8_t)val64, "3 6");
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "PRCR[0] = 0, can't modify MOMCR");
        }
        return;
    case MOSCWTCR_OFF:
        if (ra4m1_regs_clock_regs_write_allowed(s)) {
            set_with(&s->moscwtcr, (uint8_t)val64, "0 1 2 3")
        } else {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "PRCR[0] = 0, can't modify MOSCWTCR");
        }
        return;
    case SOSCCR_OFF:
        if (ra4m1_regs_clock_regs_write_allowed(s)) {
            set_with(&s->sosccr, (uint8_t)val64, "0");
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "PRCR[0] = 0, can't modify SOSCCR");
        }
        return;
    case SOMCR_OFF:
        if (ra4m1_regs_clock_regs_write_allowed(s)) {
            set_with(&s->somcr, (uint8_t)val64, "0 1");
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "PRCR[0] = 0, can't modify SOMCR");
        }
        return;
    case OPCCR_OFF:
        set_with(&s->opccr, (uint8_t)val64, "0 1 4");
        return;
    case HOCOCR_OFF:
        if (ra4m1_regs_clock_regs_write_allowed(s)) {
            set_with(&s->hococr, (uint8_t)val64, "0");
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "PRCR[0] = 0, can't modify HOCOCR");
        }
        return;
    case OSCSF_OFF:
        if (ra4m1_regs_clock_regs_write_allowed(s)) {
            set_with(&s->oscsf, (uint8_t)val64, "0 3 5");
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "PRCR[0] = 0, can't modify OSCSF");
        }
        return;
    case MEMWAIT_OFF:
        set_with(&s->memwait, (uint8_t)val64, "0");
        return;
    case USBFS_SYSCFG:
        set_with(&s->usbfs_syscfg, (uint16_t)val64, "0 3 4 5 6 8 10");
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%" HWADDR_PRIx " for regs lo\n",
                      __func__, addr);
    }
}

static const MemoryRegionOps ra4m1_regs_ops = {
    .read = ra4m1_regs_read,
    .write = ra4m1_regs_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

struct __region regions[RA4M1_REG_REGION_CNT] = {
    { .off = 0x40000000, .size = 0x70000, .shift = 0 },
    { .off = 0x40080000, .size = 0x80000, .shift = 0x80000 },
};

static void ra4m1_regs_init(Object *ob)
{
    RA4M1RegsState *s = RA4M1_REGS(ob);
    static struct region_idx indices[RA4M1_REG_REGION_CNT];
    char name[20];

    for (int i = 0; i < RA4M1_REG_REGION_CNT; i++) {
        indices[i].state = s;
        indices[i].idx = i;
        snprintf(name, sizeof(name), "%s-%d", TYPE_RA4M1_REGS, i);
        memory_region_init_io(&s->mmio[i], ob, &ra4m1_regs_ops, &indices[i],
                              name, regions[i].size);
        sysbus_init_mmio(SYS_BUS_DEVICE(ob), &s->mmio[i]);
    }
}

static const VMStateDescription vmstate_ra4m1_regs = {
    .name = TYPE_RA4M1_REGS,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields =
        (VMStateField[]){
            VMSTATE_UINT8(vbtcr1, RA4M1RegsState),
            VMSTATE_UINT8(vbtsr, RA4M1RegsState),
            VMSTATE_UINT16(prcr, RA4M1RegsState),
            VMSTATE_UINT16(fcachee, RA4M1RegsState),
            VMSTATE_UINT32(sckdivcr, RA4M1RegsState),
            VMSTATE_UINT8(sckscr, RA4M1RegsState),
            VMSTATE_UINT8(momcr, RA4M1RegsState),
            VMSTATE_UINT8(moscwtcr, RA4M1RegsState),
            VMSTATE_UINT8(sosccr, RA4M1RegsState),
            VMSTATE_UINT8(somcr, RA4M1RegsState),
            VMSTATE_UINT8(opccr, RA4M1RegsState),
            VMSTATE_UINT8(hococr, RA4M1RegsState),
            VMSTATE_UINT8(oscsf, RA4M1RegsState),
            VMSTATE_END_OF_LIST(),
        }
};

static void ra4m1_regs_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);
    dc->reset = ra4m1_regs_reset;
    dc->vmsd = &vmstate_ra4m1_regs;
}

static void ra4m1_regs_register_types(void)
{
    static const TypeInfo ra4m1_regs_info = {
        .name = TYPE_RA4M1_REGS,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(RA4M1RegsState),
        .instance_init = ra4m1_regs_init,
        .class_init = ra4m1_regs_class_init,
    };
    type_register_static(&ra4m1_regs_info);
}

type_init(ra4m1_regs_register_types);