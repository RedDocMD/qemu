#include "hw/arm/ra4m1_peripheral.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "sysemu/sysemu.h"

static void ra4m1_peripheral_init(Object *ob)
{
    RA4M1PeripheralState *s = RA4M1_PERIPHERAL(ob);

    object_initialize_child(ob, "regs", &s->regs, TYPE_RA4M1_REGS);
    object_initialize_child(ob, "flash-regs", &s->flash_regs,
                            TYPE_RA4M1_FLASH_REGS);
    int sci_idx[] = { 0, 1, 2, 9 };
    for (int i = 0; i < ARRAY_SIZE(sci_idx); i++) {
        int idx = sci_idx[i];
        char name[6];
        snprintf(name, sizeof(name), "sci-%d", idx);
        object_initialize_child(ob, name, &s->sci[idx], TYPE_RENESAS_SCI);
    }
    memory_region_init(&s->peri_mr, ob, "peripheral-mr", RA4M1_PERIPHERAL_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->peri_mr);
}

static void ra4m1_peripheral_realize(DeviceState *ds, Error **errp)
{
    RA4M1PeripheralState *s = RA4M1_PERIPHERAL(ds);

    sysbus_realize(SYS_BUS_DEVICE(&s->regs), &error_abort);
    memory_region_add_subregion(&s->peri_mr, RA4M1_REGS_LO_OFF,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->regs),
                                                       0));
    memory_region_add_subregion(&s->peri_mr, RA4M1_REGS_HI_OFF,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->regs),
                                                       1));

    sysbus_realize(SYS_BUS_DEVICE(&s->flash_regs), &error_abort);
    memory_region_add_subregion(
        &s->peri_mr, RA4M1_FLASH_REGS_OFF,
        sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->flash_regs), 0));

    int serial_mapping[] = {
        [0] = 2,
        [1] = 0,
        [2] = 1,
        [9] = 3,
    };
    int sci_idx[] = { 0, 1, 2, 9 };
    for (int i = 0; i < ARRAY_SIZE(sci_idx); i++) {
        int idx = sci_idx[i];
        qdev_prop_set_chr(DEVICE(&s->sci[idx]), "chardev",
                          serial_hd(serial_mapping[idx]));
        qdev_prop_set_uint64(DEVICE(&s->sci[idx]), "input-freq", RA4M1_CPU_HZ);
        sysbus_realize(SYS_BUS_DEVICE(&s->sci[idx]), &error_abort);

        // FIXME: Connect IRQ's
        memory_region_add_subregion(
            &s->peri_mr, RA4M1_SCI_BASE + idx * RA4M1_SCI_OFF,
            sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->sci[idx]), 0));
    }
}

static void ra4m1_peripheral_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize = ra4m1_peripheral_realize;
}

static void ra4m1_peripheral_register_types(void)
{
    static const TypeInfo ra4m1_peripheral_type_info = {
        .name = TYPE_RA4M1_PERIPHERAL,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(RA4M1PeripheralState),
        .instance_init = ra4m1_peripheral_init,
        .class_init = ra4m1_peripheral_class_init,
    };
    type_register_static(&ra4m1_peripheral_type_info);
}

type_init(ra4m1_peripheral_register_types);