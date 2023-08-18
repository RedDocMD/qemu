#include "qemu/osdep.h"

#include "hw/boards.h"
#include "qom/object.h"
#include "target/arm/cpu.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "hw/arm/armv7m.h"
#include "hw/clock.h"
#include "hw/qdev-clock.h"
#include "hw/qdev-properties.h"
#include "hw/arm/boot.h"

typedef struct RA4M1State {
    DeviceState parent_state;
    ARMv7MState armv7m;
    Clock *sysclk;
    MemoryRegion sram;
    MemoryRegion flash;
} RA4M1State;

typedef struct RA4M1Class {
    DeviceClass parent_class;
} RA4M1Class;

#define RA4M1_CPU_NAME ARM_CPU_TYPE_NAME("cortex-m4")
#define RA4M1_CORE_COUNT 1
#define RA4M1_PERIPHERAL_BASE 0x40000000
#define RA4M1_SRAM_SIZE (32 << 10)
#define RA4M1_SRAM_BASE 0x20000000
#define RA4M1_FLASH_BASE 0
#define RA4M1_FLASH_SIZE (256 << 10)
#define RA4M1_CPU_HZ (48 << 20)
#define RA4M1_NUM_IRQ 174
#define RA4M1_VT_SIZE (RA4M1_NUM_IRQ + 16)
#define DEFAULT_STACK_SIZE (1 << 10)

static void init_rom(void)
{
    MemoryRegion *sm;
    AddressSpace as;
    uint32_t vt[RA4M1_VT_SIZE];
    uint16_t instr[2];
    MemTxResult res;

    sm = get_system_memory();
    address_space_init(&as, sm, "system-memory");

    memset(vt, 0, sizeof(vt));
    vt[0] = RA4M1_SRAM_BASE + DEFAULT_STACK_SIZE; // Initial sp
    vt[1] = RA4M1_FLASH_BASE + sizeof(vt); // Instruction offset to jump to after reset

    // Required for thumb
    for (int i = 1; i < RA4M1_VT_SIZE; ++i) {
        vt[i] |= 1;
    }

    res = address_space_write_rom(&as, 0, MEMTXATTRS_UNSPECIFIED, 
                            vt, sizeof(vt));
    if (res != MEMTX_OK) {
        error_setg(&error_fatal, "failed to write to ROM");
        goto cleanup;
    }

    // loop: nop
    //       b loop
    instr[0] = 0xBF00;
    instr[1] = 0xE7FD;  

    res = address_space_write_rom(&as, RA4M1_FLASH_BASE + sizeof(vt), MEMTXATTRS_UNSPECIFIED, 
                                instr, sizeof(instr));
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
    s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
}

static void ra4m1_realize(DeviceState *dev, Error **errp)
{ 
    RA4M1State *s = RA4M1(dev);
    MemoryRegion *system_memory = get_system_memory();
    DeviceState *armv7m;

    if (!s->sysclk || !clock_has_source(s->sysclk)) {
        error_setg(errp, "sysclk clock must be wired up by the board code");
        return;
    }

    memory_region_init_ram(&s->sram, NULL, "RA4M1.sram", RA4M1_SRAM_SIZE, &error_abort);
    memory_region_add_subregion(system_memory, RA4M1_SRAM_BASE, &s->sram);

    memory_region_init_rom(&s->flash, NULL, "RA4M1.flash", RA4M1_FLASH_SIZE, &error_abort);
    memory_region_add_subregion(system_memory, RA4M1_FLASH_BASE, &s->flash);

    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    armv7m = DEVICE(&s->armv7m);
    qdev_connect_clock_in(armv7m, "cpuclk", s->sysclk);
    qdev_prop_set_string(armv7m, "cpu-type", RA4M1_CPU_NAME);
    qdev_prop_set_uint32(armv7m, "num-irq", RA4M1_NUM_IRQ);
    sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), &error_abort);
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

    init_rom();
    // To force a proper reset
    armv7m_load_kernel(ARM_CPU(first_cpu), NULL, 0, 0);
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