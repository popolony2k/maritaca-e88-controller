# Reuses the main firmware's framework-agnostic protocol/control sources
# directly from ../../src — one copy, shared by both builds, no drift risk.
set(srcs
        "main.c"
        "sketch.cpp"
        "bp32_gamepad.cpp"
        "../../src/comm/wifi_manager.cpp"
        "../../src/comm/drone_protocol.cpp"
        "../../src/comm/flow_wifi_protocol.cpp"
        "../../src/control/gamepad_controller.cpp"
        "../../src/control/accel_controller.cpp"
        "../../src/control/flight_controller.cpp")

set(requires "bluepad32" "bluepad32_arduino" "arduino" "btstack")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "." "../../src"
                    REQUIRES "${requires}")
