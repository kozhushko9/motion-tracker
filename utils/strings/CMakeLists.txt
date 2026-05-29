add_library(strings strings.cpp
)

target_include_directories(strings PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)