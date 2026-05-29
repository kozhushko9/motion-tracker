add_library(motion motion.cpp)

target_include_directories(motion PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(motion PUBLIC 
    types
    opencv_imgproc
)