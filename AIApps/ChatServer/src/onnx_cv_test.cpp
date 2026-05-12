#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include "../include/AIUtil/ImageRecognizer.h"
#include <memory>

void PredictTest(
    const std::string& model_path, const std::string& label_path,
    const std::string& image_path
) {
    std::cout << "--------- Get image model----------" << std::endl;
    
    // 创建持久化的 Ort::Env，生命周期必须比 Session 长
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "onnx_test");
    
    Ort::SessionOptions session_options;
    // 设置算子内并行数
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    auto session = std::make_unique<Ort::Session>(
        env,
        model_path.c_str(),
        session_options);
    auto allocator = std::make_unique<Ort::AllocatorWithDefaultOptions>();
    auto input_name = static_cast<std::string>(session->GetInputNameAllocated(0, *allocator).get());
    auto output_name = static_cast<std::string>(session->GetOutputNameAllocated(0, *allocator).get());
    std::vector<int64_t> input_shape = 
        session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    int input_height = input_shape[2];
    int input_width = input_shape[3];
    
    std::cout << "Model input height: " << input_height << 
        " Model input width: " << input_width << std::endl; 
    std::cout << "--------- End image model----------" << std::endl;

    std::cout << "--------- Load model data labels ----------" << std::endl;
    std::ifstream infile(label_path);
    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open label file: " + label_path);
    }
    std::string line;
    std::vector<std::string> labels; 
    while (std::getline(infile, line)) {
        if (!line.empty()) {
            labels.push_back(line);
        }
    }
    infile.close();
    if (labels.empty()) {
        throw std::runtime_error("No labels loaded from file:" + label_path);
    }
    std::cout << "--------- End load labels ----------" << std::endl;

    cv::Mat raw_img = cv::imread(image_path);
    if (raw_img.empty()) {
        std::cout << "image is empty!" << std::endl;
        return;
    }
    
    std::cout << "original image size: " << raw_img.rows 
        << " , " << raw_img.cols<< std::endl;

    cv::Mat img;
    cv::resize(raw_img, img, cv::Size(input_width, input_height));
    img.convertTo(img, CV_32F, 1.0 / 255.0);
    cv::dnn::blobFromImage(img, img);
    std::vector<int64_t> dim = {1, 3, input_height, input_width};
    size_t input_tensor_size = 1 * 3 * input_height * input_width;
    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(
        OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        mem_info, img.ptr<float>(), input_tensor_size, dim.data(), dim.size());
    
    std::cout << "input_name: " << input_name << std::endl;
    std::cout << "output_name: " << output_name << std::endl;    
    const char* input_names[] = { input_name.c_str() };
    const char* output_names[] = { output_name.c_str() };

    Ort::RunOptions run_options;
    auto output_tensors = session->Run(
        run_options,
        input_names, &input_tensor, 1,
        output_names, 1
    );
    float* output_data = output_tensors.front().GetTensorMutableData<float>();
    int num_classes = labels.empty() ? 1000 : int(labels.size());
    int pred_class = std::max_element(output_data, output_data + num_classes) - output_data;
    if (pred_class >= 0 && pred_class < (int)labels.size()) {
        std::cout << "Predict images is: " << labels[pred_class] << std::endl;
        return;
    } else {    
        std::cout << "Unknown" << std::endl;
    }
}

int main() {
    PredictTest(
        "/home/wx/Documents/my_workspace/CppAIService/models/mobilenetv2/mobilenetv2-7.onnx",
        "/home/wx/Documents/my_workspace/CppAIService/imagenet_classes.txt",
        "/home/wx/Downloads/cat.jpg"
    );

    return 0;
}


