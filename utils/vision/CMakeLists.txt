add_library(vision_utils
    vision_utils.cpp
    draw_utils.cpp
)

target_include_directories(vision_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(vision_utils PUBLIC
    opencv_core
    opencv_imgproc
    sort_tracker
)