cmake_minimum_required(VERSION 3.10)
project(onnx_cv_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

find_package(OpenCV REQUIRED)
set(ONNXRUNTIME_DIR /home/wx/Documents/my_workspace/CppAIService/third_party/onnxruntime/onnxruntime-linux-x64-1.19.2)
find_library(ONNXRUNTIME_LIBRARY onnxruntime PATH ${ONNXRUNTIME_DIR}/lib)

include_directories(
    /usr/include/opencv4
    ${ONNXRUNTIME_DIR}/include
)

set(SOURCES onnx_cv_test.cpp)
add_executable(
    onnx_cv_test
    ${SOURCES}
)
target_link_libraries(
    onnx_cv_test
    ${OPENCV4_LIBRARIES}
    /usr/lib/x86_64-linux-gnu/libopencv_core.so
    /usr/lib/x86_64-linux-gnu/libopencv_imgproc.so
    /usr/lib/x86_64-linux-gnu/libopencv_highgui.so
    /usr/lib/x86_64-linux-gnu/libopencv_imgcodecs.so
    /usr/lib/x86_64-linux-gnu/libopencv_videoio.so
    /usr/lib/x86_64-linux-gnu/libopencv_dnn.so
    ${ONNXRUNTIME_DIR}/lib/libonnxruntime.so.1.19.2
)

link_directories(${ONNXRUNTIME_DIR}/lib)