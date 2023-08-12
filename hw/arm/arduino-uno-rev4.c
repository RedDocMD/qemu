#include "qemu/osdep.h"
#include "hw/boards.h"
#include "qom/object.h"

struct ArduinoUnoRev4MachineState {
    MachineState parent_obj;
};
typedef struct ArduinoUnoRev4MachineState ArduinoUnoRev4MachineState;

struct ArduinoUnoRev4MachineClass {
    MachineClass parent_obj;
};
typedef struct ArduinoUnoRev4MachineClass ArduinoUnoRev4MachineClass;

#define TYPE_ARDUINO_UNO_REV4_MACHINE MACHINE_TYPE_NAME("arduino-uno-rev4")
DECLARE_OBJ_CHECKERS(ArduinoUnoRev4MachineState, ArduinoUnoRev4MachineClass,
                     ARDUINO_UNO_REV4_MACHINE, TYPE_ARDUINO_UNO_REV4_MACHINE)


static void arduino_uno_rev4_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->desc = "Arduino Uno Rev4";
}


static const TypeInfo arduino_uno_rev4_machine_types[] = {
    {
        .name = TYPE_ARDUINO_UNO_REV4_MACHINE,
        .parent = TYPE_MACHINE,
        .instance_size = sizeof(ArduinoUnoRev4MachineState),
        .class_size = sizeof(ArduinoUnoRev4MachineClass),
        .class_init = arduino_uno_rev4_machine_class_init,
    }
};

DEFINE_TYPES(arduino_uno_rev4_machine_types)