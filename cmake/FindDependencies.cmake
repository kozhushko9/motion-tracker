include_guard(GLOBAL)

message(STATUS "Dependency mode: Raspberry Pi Zero 2W / aarch64")

find_package(Threads REQUIRED)
find_package(yaml-cpp REQUIRED)

# OpenCV
set(OPENCV_INSTALL_ROOT
    "${CMAKE_SOURCE_DIR}/third_party/opencv/aarch64-linux-gnu"
)

set(OpenCV_DIR
    "${OPENCV_INSTALL_ROOT}/lib/cmake/opencv4"
    CACHE PATH "Path to Pi OpenCVConfig.cmake"
    FORCE
)

find_package(OpenCV REQUIRED COMPONENTS
    core
    imgproc
    imgcodecs
    videoio
    highgui
    video
)

install(
    DIRECTORY "${OPENCV_INSTALL_ROOT}/lib/"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    FILES_MATCHING PATTERN "*.so*"
)

# TensorFlow Lite
set(TFLITE_ROOT
    "${CMAKE_SOURCE_DIR}/third_party/tflite/aarch64-linux-gnu"
)

set(TFLITE_INCLUDE_DIR "${TFLITE_ROOT}/include")
set(TFLITE_LIBRARY "${TFLITE_ROOT}/lib/libtensorflowlite.so")

if(NOT EXISTS "${TFLITE_INCLUDE_DIR}/tensorflow/lite/interpreter.h")
    message(FATAL_ERROR "TensorFlow Lite headers not found: ${TFLITE_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${TFLITE_LIBRARY}")
    message(FATAL_ERROR "TensorFlow Lite library not found: ${TFLITE_LIBRARY}")
endif()

add_library(TensorFlowLite::tensorflowlite SHARED IMPORTED GLOBAL)

set_target_properties(TensorFlowLite::tensorflowlite PROPERTIES
    IMPORTED_LOCATION "${TFLITE_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${TFLITE_INCLUDE_DIR}"
)

install(
    FILES "${TFLITE_LIBRARY}"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "TensorFlow Lite library: ${TFLITE_LIBRARY}")