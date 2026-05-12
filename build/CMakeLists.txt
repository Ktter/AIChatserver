cmake_minimum_required(VERSION 3.10)
project(http_server)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# 查找 OpenSSL 和 CURL
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# 添加头文件路径
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/HttpServer/include
    ${PROJECT_SOURCE_DIR}/AIApps/ChatServer/include
    /usr/include/mysql-cppconn-8
    /usr/include/cppconn
    /usr/include/mysql
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    /usr/include/opencv4          # OpenCV 头文件路径
    /usr/local/include            # SimpleAmqpClient & ONNX Runtime
    /home/wx/muduo               # muduo 源码目录
    ${PROJECT_SOURCE_DIR}/third_party/onnxruntime/onnxruntime-linux-x64-1.19.2/include  # ONNX Runtime
    ${PROJECT_SOURCE_DIR}/third_party/SimpleAmqpClient/src  # SimpleAmqpClient
)

# 查找必要的库
find_library(MYSQLCPPCONN_LIBRARY
    NAMES mysqlcppconn mysqlcppconn8
    PATHS /usr/lib /usr/lib64 /usr/local/lib
)

# muduo 库
find_library(MUDUO_NET_LIBRARY
    NAMES muduo_net
    PATHS /home/wx/build/release-cpp11/lib
)

find_library(MUDUO_BASE_LIBRARY
    NAMES muduo_base
    PATHS /home/wx/build/release-cpp11/lib
)

# ONNX Runtime (from third_party)
find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    PATHS ${PROJECT_SOURCE_DIR}/third_party/onnxruntime/onnxruntime-linux-x64-1.19.2/lib
)
if(ONNXRUNTIME_LIBRARY)
    message(STATUS "ONNX Runtime found: ${ONNXRUNTIME_LIBRARY}")
else()
    message(STATUS "ONNX Runtime not found, skipping...")
endif()

# SimpleAmqpClient (from third_party)
find_library(SIMPLEAMQPCLIENT_LIBRARY
    NAMES SimpleAmqpClient
    PATHS ${PROJECT_SOURCE_DIR}/third_party/SimpleAmqpClient
)
if(SIMPLEAMQPCLIENT_LIBRARY)
    message(STATUS "SimpleAmqpClient found: ${SIMPLEAMQPCLIENT_LIBRARY}")
else()
    message(STATUS "SimpleAmqpClient not found, using librabbitmq instead")
endif()

# 添加源文件
file(GLOB_RECURSE HTTP_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/HttpServer/src/*.cpp"
)

file(GLOB_RECURSE GOMOKU_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/AIApps/ChatServer/src/*.cpp"
)

# 排除测试文件和 build 目录
list(REMOVE_ITEM GOMOKU_SERVER_SRC 
    "${PROJECT_SOURCE_DIR}/AIApps/ChatServer/src/onnx_cv_test.cpp"
)
# 排除 build 目录下的文件
foreach(src_file ${GOMOKU_SERVER_SRC})
    if(src_file MATCHES "/build/")
        list(REMOVE_ITEM GOMOKU_SERVER_SRC ${src_file})
    endif()
endforeach()

# 主程序文件
set(MAIN_SRC "${PROJECT_SOURCE_DIR}/AIApps/ChatServer/src/main.cpp")

# 添加可执行文件
add_executable(http_server
    ${MAIN_SRC}
    ${HTTP_SERVER_SRC}
    ${GOMOKU_SERVER_SRC}
)

# 链接库
target_link_libraries(http_server
    pthread
    ${MUDUO_NET_LIBRARY}
    ${MUDUO_BASE_LIBRARY}
    mysqlcppconn
    mysqlclient
    ssl
    crypto
    ${CURL_LIBRARIES}
    ${OPENCV4_LIBRARIES}
    /usr/lib/x86_64-linux-gnu/libopencv_core.so
    /usr/lib/x86_64-linux-gnu/libopencv_imgproc.so
    /usr/lib/x86_64-linux-gnu/libopencv_highgui.so
    /usr/lib/x86_64-linux-gnu/libopencv_imgcodecs.so
    /usr/lib/x86_64-linux-gnu/libopencv_videoio.so
    /usr/lib/x86_64-linux-gnu/libopencv_dnn.so
)

# 条件链接 ONNX Runtime
if(ONNXRUNTIME_LIBRARY)
    target_link_libraries(http_server ${ONNXRUNTIME_LIBRARY})
endif()

# 条件链接 SimpleAmqpClient 或 librabbitmq
if(SIMPLEAMQPCLIENT_LIBRARY)
    target_link_libraries(http_server ${SIMPLEAMQPCLIENT_LIBRARY})
else()
    target_link_libraries(http_server rabbitmq)
endif()

# 如果库不在默认路径，添加链接目录
link_directories(
    /usr/local/lib
    ${PROJECT_SOURCE_DIR}/third_party/onnxruntime/onnxruntime-linux-x64-1.19.2/lib
)

# 打印调试信息
message(STATUS "Include directories:")
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
    message(STATUS "  ${dir}")
endforeach()

message(STATUS "ONNX Runtime library: ${ONNXRUNTIME_LIBRARY}")
message(STATUS "Main source file: ${MAIN_SRC}")
message(STATUS "HTTP Server sources: ${HTTP_SERVER_SRC}")
message(STATUS "Gomoku Server sources: ${GOMOKU_SERVER_SRC}")
