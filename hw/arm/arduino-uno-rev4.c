#include "qemu/osdep.h"

#include "hw/boards.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "hw/qdev-clock.h"
#include "hw/arm/boot.h"
#include "hw/arm/ra4m1.h"

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
