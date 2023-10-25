#include "hw/intc/ra4m1_icu.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "hw/char/renesas_sci.h"

#define IELSR_LO 0x300
#define IELSR_HI 0x37C

static const struct {
    uint8_t mask;
    int idx;
    int irq;
} sci_int_table[] = {
    { .mask = 0x98, .idx = 0, .irq = RXI },
    { .mask = 0x99, .idx = 0, .irq = TXI },
    { .mask = 0x9A, .idx = 0, .irq = TEI },
    { .mask = 0x9B, .idx = 0, .irq = ERI },
    { .mask = 0x9E, .idx = 1, .irq = RXI },
    { .mask = 0x9F, .idx = 1, .irq = TXI },
    { .mask = 0xA0, .idx = 1, .irq = TEI },
    { .mask = 0xA1, .idx = 1, .irq = ERI },
    { .mask = 0xA3, .idx = 2, .irq = RXI },
    { .mask = 0xA4, .idx = 2, .irq = TXI },
    { .mask = 0xA4, .idx = 2, .irq = TEI },
    { .mask = 0xA5, .idx = 2, .irq = ERI },
    { .mask = 0xA8, .idx = 9, .irq = RXI },
    { .mask = 0xA9, .idx = 9, .irq = TXI },
    { .mask = 0xAA, .idx = 9, .irq = TEI },
    { .mask = 0xAB, .idx = 9, .irq = ERI },
};

static void ra4m1_icu_reset(DeviceState *dev)
{
    RA4M1IcuState *s = RA4M1_ICU(dev);
    bzero(s->ielsr, sizeof(s->ielsr));
}

static void ra4m1_icu_write(void *opaque, hwaddr offset, uint64_t val,
                            unsigned size)
{
    RA4M1IcuState *s = opaque;
    int idx, sci_idx, irq_idx;
    uint8_t event;
    Object *ob, *cpu;
    RSCIState *sci;
    qemu_irq irq;

    if (offset >= IELSR_LO && offset <= IELSR_HI) {
        idx = (offset - IELSR_LO) / sizeof(uint32_t);
        s->ielsr[idx] = (uint32_t)val;
        event = val & 0xFF;

        cpu = object_property_get_link(OBJECT(s), "cpu", &error_abort);
        ob = object_property_get_link(OBJECT(s), "sci", &error_abort);
        sci = RSCI(ob);

        for (int i = 0; i < ARRAY_SIZE(sci_int_table); i++) {
            if (sci_int_table[i].mask == event) {
                sci_idx = sci_int_table[i].idx;
                irq_idx = sci_int_table[i].irq;
                irq = qdev_get_gpio_in(DEVICE(cpu), idx);
                sysbus_connect_irq(SYS_BUS_DEVICE(&sci[sci_idx]), irq_idx, irq);
                break;
            }
        }
        return;
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: Bad offset 0x%" HWADDR_PRIx " for ICU\n", __func__,
                  offset);
}

static uint64_t ra4m1_icu_read(void *opaque, hwaddr offset, unsigned size)
{
    RA4M1IcuState *s = opaque;

    if (offset >= IELSR_LO && offset <= IELSR_HI) {
        int idx = (offset - IELSR_LO) / sizeof(uint32_t);
        return s->ielsr[idx];
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: Bad offset 0x%" HWADDR_PRIx " for ICU\n", __func__,
                  offset);
    return UINT64_MAX;
}

static void ra4m1_icu_init(Object *ob)
{
    RA4M1IcuState *s = RA4M1_ICU(ob);
    static const MemoryRegionOps ra4m1_icu_ops = {
        .write = ra4m1_icu_write,
        .read = ra4m1_icu_read,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    memory_region_init_io(&s->mmio, ob, &ra4m1_icu_ops, s, "ra4m1-icu",
                          RA4M1_ICU_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio);
}

static void ra4m1_icu_realize(DeviceState *dev, Error **errp)
{
}

static void ra4m1_icu_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);

    dc->realize = ra4m1_icu_realize;
    dc->reset = ra4m1_icu_reset;
}

static void ra4m1_icu_register_types(void)
{
    static const TypeInfo ra4m1_icu_info = {
        .name = TYPE_RA4M1_ICU,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(RA4M1IcuState),
        .instance_init = ra4m1_icu_init,
        .class_init = ra4m1_icu_class_init,
    };
    type_register_static(&ra4m1_icu_info);
}

type_init(ra4m1_icu_register_types)