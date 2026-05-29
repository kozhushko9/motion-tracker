add_library(metrics
    metrics.cpp
    scoped_timer.cpp
    fps_counter.cpp
)

target_include_directories(metrics PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(metrics PUBLIC config)
