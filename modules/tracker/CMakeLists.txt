add_library(sort_tracker sort_tracker.cpp)

target_include_directories(sort_tracker PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sort_tracker PUBLIC
    vision_utils
    opencv_video
)