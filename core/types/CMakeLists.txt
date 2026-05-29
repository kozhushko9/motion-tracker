add_library(types INTERFACE)

target_include_directories(types INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(types INTERFACE
    opencv_core
)
