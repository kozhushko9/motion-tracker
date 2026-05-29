add_library(core pipeline.cpp)

target_include_directories(core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(core PUBLIC
    strings
    motion
    detector
    sort_tracker
    config
    metrics
    containers
    vision_utils
    opencv_videoio
    opencv_highgui
    Threads::Threads
)