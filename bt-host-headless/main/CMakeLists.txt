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
        "../../src/control/flight_controller.cpp"
        "../../src/hal/m5stickcplus2.cpp"
        "../../src/ui/display.cpp")

set(requires "bluepad32" "bluepad32_arduino" "arduino" "btstack" "M5Unified" "M5GFX")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "." "../../src"
                    REQUIRES "${requires}")

# src/ui/display.cpp picks its 240px-wide StickC Plus2 layout vs. the AtomS3's
# 128px default via this macro (see platformio.ini's build_flags for the main
# firmware's StickC Plus2 environment, which sets the same define).
target_compile_definitions(${COMPONENT_LIB} PRIVATE BOARD_STICKC_PLUS2)
