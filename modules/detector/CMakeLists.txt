add_library(detector detector.cpp)

target_include_directories(detector PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(detector
    PUBLIC 
        types
        config
        metrics
        vision_utils
        TensorFlowLite::tensorflowlite
)