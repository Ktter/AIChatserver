#include <iostream>
#include <onnxruntime_cxx_api.h>

int main() {
    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "test");
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        Ort::Session session(env, "models/mobilenetv2/mobilenetv2-7.onnx", session_options);
        
        std::cout << "Getting type info..." << std::endl;
        auto type_info = session.GetInputTypeInfo(0);
        std::cout << "Getting tensor info..." << std::endl;
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        std::cout << "Getting shape..." << std::endl;
        auto shape = tensor_info.GetShape();
        std::cout << "Shape size: " << shape.size() << std::endl;
        for (size_t i = 0; i < shape.size(); ++i) {
            std::cout << "  dim[" << i << "] = " << shape[i] << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
