#include "hw/arm/ra4m1_regs.h"
#include "qemu/log.h"

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
