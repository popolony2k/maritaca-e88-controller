# Thin CMake wrapper around PlatformIO — for tooling/IDEs that expect a
# standard CMake interface. This does not replace platformio.ini or
# duplicate the build in any way; every target below just shells out to
# `pio`. PlatformIO remains the actual build system; build it directly
# with `pio run` if you prefer (see README.md).
cmake_minimum_required(VERSION 3.16)
project(maritaca-e88-controller NONE)

set(PIO_ENV "m5stack-atoms3" CACHE STRING "PlatformIO environment to build")

find_program(PIO_EXECUTABLE pio
    HINTS "$ENV{HOME}/.platformio/penv/bin"
)
if(NOT PIO_EXECUTABLE)
    message(FATAL_ERROR "PlatformIO 'pio' executable not found. Install PlatformIO Core or add it to PATH.")
endif()

# `cmake --build <dir>` with no target builds the firmware (ALL).
add_custom_target(build ALL
    COMMAND ${PIO_EXECUTABLE} run -e ${PIO_ENV}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building firmware via PlatformIO (env: ${PIO_ENV})"
    VERBATIM
)

# `cmake --build <dir> --target upload`
add_custom_target(upload
    COMMAND ${PIO_EXECUTABLE} run -e ${PIO_ENV} -t upload
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Flashing firmware via PlatformIO (env: ${PIO_ENV})"
    VERBATIM
)

# `cmake --build <dir> --target monitor`
add_custom_target(monitor
    COMMAND ${PIO_EXECUTABLE} device monitor -e ${PIO_ENV}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Opening Serial Monitor via PlatformIO (env: ${PIO_ENV})"
    USES_TERMINAL
    VERBATIM
)

# `cmake --build <dir> --target clean-pio` (named to avoid colliding with
# CMake's own built-in "clean" target).
add_custom_target(clean-pio
    COMMAND ${PIO_EXECUTABLE} run -e ${PIO_ENV} -t clean
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Cleaning PlatformIO build artifacts (env: ${PIO_ENV})"
    VERBATIM
)
