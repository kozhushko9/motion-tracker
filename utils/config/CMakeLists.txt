add_library(config config.cpp)

target_include_directories(config PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(config PUBLIC 
    yaml-cpp::yaml-cpp
)
