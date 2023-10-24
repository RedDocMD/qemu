#include "hw/intc/ra4m1_icu.h"

static void ra4m1_icu_reset(DeviceState *dev)
{
}

static void ra4m1_icu_write(void *opaque, hwaddr offset, uint64_t val,
                            unsigned size)
{
}

static uint64_t ra4m1_icu_read(void *opaque, hwaddr offset, unsigned size)
{
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