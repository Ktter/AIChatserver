#include <iostream>
#include <onnxruntime_cxx_api.h>

int main() {
    try {
        std::cout << "Creating Ort::Env..." << std::endl;
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "test");
        std::cout << "Env created OK" << std::endl;
        
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        
        std::cout << "Creating Session..." << std::endl;
        Ort::Session session(env, "models/mobilenetv2/mobilenetv2-7.onnx", session_options);
        std::cout << "Session created OK" << std::endl;
        
        std::cout << "Input count: " << session.GetInputCount() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
