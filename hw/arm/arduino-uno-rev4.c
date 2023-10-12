#include "qemu/osdep.h"

#include "hw/boards.h"
#include "qom/object.h"
#include "sysemu/sysemu.h"
#include "target/arm/cpu.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "hw/arm/armv7m.h"
#include "hw/clock.h"
#include "hw/qdev-clock.h"
#include "hw/qdev-properties.h"
#include "hw/arm/boot.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "hw/char/renesas_sci.h"
#include <stdarg.h>

#define set_bit_from(dest, src, bit)     \
    do {                                 \
        *(dest) &= ~(1 << (bit));        \
        *(dest) |= (1 << (bit)) & (src); \
    } while (0);

#define set_with_retain(old, new, retain)                                      \
  do {                                                                         \
    uint64_t val = (new);                                                      \
    int bit;                                                                   \
    int off = 0, read = 0;                                                     \
    while ((sscanf((char *)((uint8_t *)retain + off), "%d%n", &bit, &read) !=  \
            EOF)) {                                                            \
      off += read;                                                             \
      set_bit_from(&val, *(old), bit);                                         \
    }                                                                          \
    *(old) = val;                                                              \
  } while (0);

#define set_with(old, new, with)                                               \
  do {                                                                         \
    int bit;                                                                   \
    int off = 0, read = 0;                                                     \
    while ((sscanf((char *)((uint8_t *)with + off), "%d%n", &bit, &read) !=    \
            EOF)) {                                                            \
      off += read;                                                             \
      set_bit_from((old), (new), bit);                                         \
    }                                                                          \
  } while (0);

#define FIELD_SIZEOF(t, f) (sizeof(((t *)0)->f))

#define RA4M1_REGS_OFF 0x40000000
#define RA4M1_REGS_SIZE 0x100000

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
#define PCNTR_CNT        10
#define PCNTR_SHIFT      0x20
// clang-format on

#define TYPE_RA4M1_REGS "ra4m1-regs"
OBJECT_DECLARE_SIMPLE_TYPE(RA4M1RegsState, RA4M1_REGS)

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
    MemoryRegion mmio;

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

static uint64_t ra4m1_regs_read(void *opaque, hwaddr addr, unsigned int size)
{
    RA4M1RegsState *s = opaque;
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
                      "%s: Bad offset 0x%" HWADDR_PRIx " for reg\n", __func__,
                      addr);
        return 0;
    }
}

static void ra4m1_regs_write(void *opaque, hwaddr addr, uint64_t val64,
                             unsigned int size)
{
    RA4M1RegsState *s = opaque;
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
                      "%s: Bad offset 0x%" HWADDR_PRIx " for regs\n", __func__,
                      addr);
    }
}

static const MemoryRegionOps ra4m1_regs_ops = {
    .read = ra4m1_regs_read,
    .write = ra4m1_regs_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ra4m1_regs_init(Object *ob)
{
    RA4M1RegsState *s = RA4M1_REGS(ob);

    memory_region_init_io(&s->mmio, ob, &ra4m1_regs_ops, s, TYPE_RA4M1_REGS,
                          RA4M1_REGS_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(ob), &s->mmio);
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

#define RA4M1_FLASH_REGS_OFF 0x407E0000
#define RA4M1_FLASH_REGS_SIZE 0x10000

#define TYPE_RA4M1_FLASH_REGS "ra4m1-flash-regs"
OBJECT_DECLARE_SIMPLE_TYPE(RA4M1FlashRegsState, RA4M1_FLASH_REGS)

typedef struct RA4M1FlashRegsState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
} RA4M1FlashRegsState;

static void ra4m1_flash_regs_reset(DeviceState *dev)
{
    // RA4M1RegsState *s = RA4M1_FLASH_REGS(dev);
}

static uint64_t ra4m1_flash_regs_read(void *opaque, hwaddr addr,
                                      unsigned int size)
{
    // RA4M1RegsState *s = RA4M1_FLASH_REGS(dev);

    switch (addr) {
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%" HWADDR_PRIx " for flash reg\n",
                      __func__, addr);
        return 0;
    }
}

static void ra4m1_flash_regs_write(void *opaque, hwaddr addr, uint64_t val64,
                                   unsigned int size)
{
    // RA4M1RegsState *s = RA4M1_FLASH_REGS(dev);

    switch (addr) {
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%" HWADDR_PRIx " for flash reg\n",
                      __func__, addr);
    }
}

static const MemoryRegionOps ra4m1_flash_regs_ops = {
    .read = ra4m1_flash_regs_read,
    .write = ra4m1_flash_regs_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ra4m1_flash_regs_init(Object *ob)
{
    RA4M1FlashRegsState *s = RA4M1_FLASH_REGS(ob);

    memory_region_init_io(&s->mmio, ob, &ra4m1_flash_regs_ops, s,
                          TYPE_RA4M1_FLASH_REGS, RA4M1_FLASH_REGS_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(ob), &s->mmio);
}

static void ra4m1_flash_regs_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);
    dc->reset = ra4m1_flash_regs_reset;
}

static void ra4m1_flash_regs_register_types(void)
{
    static const TypeInfo ra4m1_flash_regs_info = {
        .name = TYPE_RA4M1_FLASH_REGS,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(RA4M1FlashRegsState),
        .instance_init = ra4m1_flash_regs_init,
        .class_init = ra4m1_flash_regs_class_init,
    };
    type_register_static(&ra4m1_flash_regs_info);
}

type_init(ra4m1_flash_regs_register_types);

typedef struct RA4M1State {
    DeviceState parent_state;
    ARMv7MState armv7m;
    Clock *sysclk;
    MemoryRegion sram;
    MemoryRegion flash;
    MemoryRegion onchip_flash;
    RA4M1RegsState regs;
    RA4M1FlashRegsState flash_regs;
    RSCIState sci;
} RA4M1State;

typedef struct RA4M1Class {
    DeviceClass parent_class;
} RA4M1Class;

#define RA4M1_CPU_NAME ARM_CPU_TYPE_NAME("cortex-m4")
#define RA4M1_CORE_COUNT 1
#define RA4M1_PERIPHERAL_BASE 0x40000000
#define RA4M1_SCI_BASE 0x40070000
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

static void init_rom(void)
{
    MemoryRegion *sm;
    AddressSpace as;
    uint32_t vt[RA4M1_VT_SIZE] = { 0 };
    MemTxResult res;

    sm = get_system_memory();
    address_space_init(&as, sm, "system-memory");

    vt[0] = RA4M1_SRAM_BASE + DEFAULT_STACK_SIZE; // Initial sp
    vt[1] = RA4M1_FLASH_BASE +
            sizeof(vt); // Instruction offset to jump to after reset

    // Required for thumb
    for (int i = 1; i < RA4M1_VT_SIZE; ++i) {
        vt[i] |= 1;
    }

    res =
        address_space_write_rom(&as, 0, MEMTXATTRS_UNSPECIFIED, vt, sizeof(vt));
    if (res != MEMTX_OK) {
        error_setg(&error_fatal, "failed to write to ROM");
        goto cleanup;
    }

    // uint16_t instr[] = {
    //     0xbf00, // loop: nop
    //     0xe7fd, //       b loop
    // };
    uint16_t instr[] = {
        0x4801, 0x6801, 0xbf00, 0xe7fd, 0xe41f, 0x4001,
    };

    res = address_space_write_rom(&as, RA4M1_FLASH_BASE + sizeof(vt),
                                  MEMTXATTRS_UNSPECIFIED, instr, sizeof(instr));
    if (res != MEMTX_OK) {
        error_setg(&error_fatal, "failed to write to ROM");
        goto cleanup;
    }

cleanup:
    address_space_destroy(&as);
}

#define TYPE_RA4M1 "ra4m1"
OBJECT_DECLARE_TYPE(RA4M1State, RA4M1Class, RA4M1)

static void ra4m1_init(Object *ob)
{
    RA4M1State *s = RA4M1(ob);

    object_initialize_child(ob, "armv7m", &s->armv7m, TYPE_ARMV7M);
    object_initialize_child(ob, "regs", &s->regs, TYPE_RA4M1_REGS);
    object_initialize_child(ob, "flash-regs", &s->flash_regs,
                            TYPE_RA4M1_FLASH_REGS);
    object_initialize_child(ob, "sci", &s->sci, TYPE_RENESAS_SCI);
    s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
}

static void ra4m1_realize(DeviceState *ds, Error **errp)
{
    RA4M1State *s = RA4M1(ds);
    MemoryRegion *system_memory = get_system_memory();
    DeviceState *armv7m, *dev;
    SysBusDevice *busdev;

    if (!s->sysclk || !clock_has_source(s->sysclk)) {
        error_setg(errp, "sysclk clock must be wired up by the board code");
        return;
    }

    memory_region_init_ram(&s->sram, NULL, "RA4M1.sram", RA4M1_SRAM_SIZE,
                           &error_abort);
    memory_region_add_subregion(system_memory, RA4M1_SRAM_BASE, &s->sram);

    memory_region_init_rom(&s->flash, NULL, "RA4M1.flash", RA4M1_FLASH_SIZE,
                           &error_abort);
    memory_region_add_subregion(system_memory, RA4M1_FLASH_BASE, &s->flash);
    memory_region_init_rom(&s->onchip_flash, NULL, "RA4M1.onchip_flash",
                           RA4M1_ONCHIP_FLASH_SIZE, &error_abort);
    memory_region_add_subregion(system_memory, RA4M1_ONCHIP_FLASH_BASE,
                                &s->onchip_flash);

    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    armv7m = DEVICE(&s->armv7m);
    qdev_connect_clock_in(armv7m, "cpuclk", s->sysclk);
    qdev_prop_set_string(armv7m, "cpu-type", RA4M1_CPU_NAME);
    qdev_prop_set_uint32(armv7m, "num-irq", RA4M1_NUM_IRQ);
    sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), &error_abort);

    dev = DEVICE(&s->regs);
    sysbus_realize(SYS_BUS_DEVICE(&s->regs), &error_abort);
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, RA4M1_REGS_OFF);

    dev = DEVICE(&s->flash_regs);
    sysbus_realize(SYS_BUS_DEVICE(&s->flash_regs), &error_abort);
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, RA4M1_FLASH_REGS_OFF);

    dev = DEVICE(&s->sci);
    busdev = SYS_BUS_DEVICE(dev);
    qdev_prop_set_chr(dev, "chardev", serial_hd(1));
    qdev_prop_set_uint64(dev, "input-freq", RA4M1_CPU_HZ);
    sysbus_realize(busdev, &error_abort);

    // FIXME: Connect IRQ's
    sysbus_mmio_map(busdev, 0, RA4M1_SCI_BASE);
}

static void ra4m1_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize = ra4m1_realize;
}

static void ra4m1_register_types(void)
{
    static const TypeInfo ra4m1_type_info = {
        .name = TYPE_RA4M1,
        .parent = TYPE_DEVICE,
        .instance_size = sizeof(RA4M1State),
        .instance_init = ra4m1_init,
        .class_size = sizeof(RA4M1Class),
        .class_init = ra4m1_class_init,
    };
    type_register_static(&ra4m1_type_info);
}

type_init(ra4m1_register_types);

typedef struct ArduinoUnoRev4MachineState {
    MachineState parent_state;
    RA4M1State soc;
} ArduinoUnoRev4MachineState;

typedef struct ArduinoUnoRev4MachineClass {
    MachineClass parent_class;
} ArduinoUnoRev4MachineClass;

#define TYPE_ARDUINO_UNO_REV4_MACHINE MACHINE_TYPE_NAME("arduino-uno-rev4")
DECLARE_OBJ_CHECKERS(ArduinoUnoRev4MachineState, ArduinoUnoRev4MachineClass,
                     ARDUINO_UNO_REV4_MACHINE, TYPE_ARDUINO_UNO_REV4_MACHINE)

static void arduino_uno_rev4_machine_init(MachineState *ms)
{
    ArduinoUnoRev4MachineState *s = ARDUINO_UNO_REV4_MACHINE(ms);
    Clock *sysclk;

    if (ms->ram_size != RA4M1_SRAM_SIZE) {
        char *size_str = size_to_str(RA4M1_SRAM_SIZE);
        error_report("Invalid RAM size, should be %s", size_str);
        g_free(size_str);
        exit(1);
    }

    sysclk = clock_new(OBJECT(ms), "sysclk");
    clock_set_hz(sysclk, RA4M1_CPU_HZ);

    // memory_region_add_subregion_overlap(get_system_memory(), 0, ms->ram, 0);

    object_initialize_child(OBJECT(ms), "soc", &s->soc, TYPE_RA4M1);
    object_property_add_const_link(OBJECT(&s->soc), "ram", OBJECT(ms->ram));
    qdev_connect_clock_in(DEVICE(&s->soc), "sysclk", sysclk);
    qdev_realize(DEVICE(&s->soc), NULL, &error_fatal);

    // Default ROM state
    if (!ms->bootloader_filename && !ms->kernel_filename) {
        init_rom();
    }

    armv7m_load_bootloader(ARM_CPU(first_cpu), ms->bootloader_filename);
    armv7m_load_hex_kernel(ARM_CPU(first_cpu), ms->kernel_filename);
}

static void arduino_uno_rev4_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->desc = "Arduino Uno Rev4";
    mc->init = arduino_uno_rev4_machine_init;
    mc->no_parallel = 1;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->default_cpus = mc->min_cpus = mc->max_cpus = RA4M1_CORE_COUNT;
    mc->default_ram_size = RA4M1_SRAM_SIZE;
    mc->default_ram_id = "ram";
}

static const TypeInfo arduino_uno_rev4_machine_types[] = {
    {
        .name = TYPE_ARDUINO_UNO_REV4_MACHINE,
        .parent = TYPE_MACHINE,
        .instance_size = sizeof(ArduinoUnoRev4MachineState),
        .class_size = sizeof(ArduinoUnoRev4MachineClass),
        .class_init = arduino_uno_rev4_machine_class_init,
    },
};

DEFINE_TYPES(arduino_uno_rev4_machine_types)
