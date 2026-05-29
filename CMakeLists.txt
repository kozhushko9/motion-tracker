cmake_minimum_required(VERSION 3.20)

# Enable ccache globally
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER   ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

project(motion_tracker
    VERSION 0.1.0
    LANGUAGES C CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compile database generation (clangd, static analysis)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_CROSSCOMPILING)
    message(STATUS "Native configure detected; Pi cross-toolchain is recommended")
endif()

set(APP_CONFIG_FILE "${CMAKE_SOURCE_DIR}/utils/config/config.yaml")

include(GNUInstallDirs)

# Dependencies
include(cmake/FindDependencies.cmake)

# Runtime library lookup path for installed executable
set(CMAKE_INSTALL_RPATH 
    "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/aarch64-linux-gnu"
)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_SKIP_RPATH FALSE)
set(CMAKE_SKIP_INSTALL_RPATH FALSE)

set(APP_TARGET "motion_tracker")
set(APP_ID "motion-tracker")

# Warnings
add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

# Libraries
add_subdirectory(core/types)

add_subdirectory(utils/paths)
add_subdirectory(utils/strings)
add_subdirectory(utils/config)
add_subdirectory(utils/metrics)
add_subdirectory(utils/vision)
add_subdirectory(utils/containers)

add_subdirectory(modules/motion)
add_subdirectory(modules/detector)
add_subdirectory(modules/tracker)

add_subdirectory(core)

# Define an executable target
add_executable(${APP_TARGET} apps/main.cpp)

# Link libraries (the rest is transient from core)
target_link_libraries(${APP_TARGET} PRIVATE 
    core 
    paths
)

# Install rules
install(
    TARGETS ${APP_TARGET} 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(
    FILES "${APP_CONFIG_FILE}" 
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${APP_ID}
    RENAME config.yaml
)
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/models/"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${APP_ID}/models
    FILES_MATCHING
    PATTERN "*.tflite"
)

# Symlink compile_commands.json from build dir to project root
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(symlink_compile_commands ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_SOURCE_DIR}/compile_commands.json
    )
endif()