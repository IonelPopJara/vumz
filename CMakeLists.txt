cmake_minimum_required(VERSION 3.10)
project(vumz)

# Specify the C standard
set(CMAKE_C_STANDARD 99)

# Generate compile_comomands.json for LSP support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#define source files
set (SRC_DIR src)
set (SOURCES
    ${SRC_DIR}/main.c
    ${SRC_DIR}/audio-cap.c
    ${SRC_DIR}/audio-out.c
)

# Set the output directory for the binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)

# Include directories
include_directories(/usr/include/pipewire-0.3 /usr/include/spa-0.2)

# Add the executable
add_executable(vumz ${SOURCES})

# Link libraries
target_link_libraries(vumz m ncursesw pipewire-0.3)

# Installation rules
install(TARGETS vumz RUNTIME DESTINATION /usr/local/bin)

# Install man page
install(FILES doc/vumz.1 DESTINATION /usr/local/share/man/man1)

# Uninstall target
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -E remove -f /usr/local/bin/vumz
    COMMAND ${CMAKE_COMMAND} -E remove -f /usr/local/share/man/man1/vumz.1
    COMMENT "Uninstalling Vumz..."
)

add_custom_command(TARGET uninstall
    POST_BUILD
    COMMAND echo "Vumz was succesfully uninstalled"
)

