#include "hw/arm/ra4m1.h"
#include "qapi/error.h"
#include "hw/qdev-clock.h"
#include "hw/qdev-properties.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"

static void ra4m1_init(Object *ob)
{
    RA4M1State *s = RA4M1(ob);

    object_initialize_child(ob, "armv7m", &s->armv7m, TYPE_ARMV7M);
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
    sysbus_mmio_map(busdev, 0, RA4M1_REGS_LO_OFF);
    sysbus_mmio_map(busdev, 1, RA4M1_REGS_HI_OFF);

    dev = DEVICE(&s->flash_regs);
    sysbus_realize(SYS_BUS_DEVICE(&s->flash_regs), &error_abort);
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, RA4M1_FLASH_REGS_OFF);

    int serial_mapping[] = {
        [0] = 2,
        [1] = 0,
        [2] = 1,
        [9] = 3,
    };
    int sci_idx[] = { 0, 1, 2, 9 };
    for (int i = 0; i < ARRAY_SIZE(sci_idx); i++) {
        int idx = sci_idx[i];
        busdev = SYS_BUS_DEVICE(&s->sci[idx]);
        qdev_prop_set_chr(DEVICE(busdev), "chardev",
                          serial_hd(serial_mapping[idx]));
        qdev_prop_set_uint64(DEVICE(busdev), "input-freq", RA4M1_CPU_HZ);
        sysbus_realize(busdev, &error_abort);

        // FIXME: Connect IRQ's
        sysbus_mmio_map(busdev, 0, RA4M1_SCI_BASE + idx * RA4M1_SCI_OFF);
    }
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