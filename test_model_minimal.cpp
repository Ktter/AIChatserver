#include <iostream>
#include <memory>
#include <cstring>
#include <onnxruntime_cxx_api.h>

class ModelInference {
public:
    ModelInference(const std::string& model_path, const std::string& model_name)
    : env_(ORT_LOGGING_LEVEL_WARNING, model_name.c_str())
    , model_name_(model_name)
    , model_path_(model_path)
    {
        std::cout << "[1] Creating session options..." << std::endl;
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        
        std::cout << "[2] Creating session..." << std::endl;
        session_ = std::make_unique<Ort::Session>(env_, model_path_.c_str(), session_options);
        
        std::cout << "[3] Creating allocator..." << std::endl;
        allocator_ = std::make_unique<Ort::AllocatorWithDefaultOptions>();
        
        std::cout << "[4] Getting input name..." << std::endl;
        auto in_allocated = session_->GetInputNameAllocated(0, *allocator_);
        input_name_ = std::string(in_allocated.get());
        
        std::cout << "[5] Getting input shape..." << std::endl;
        auto type_info = session_->GetInputTypeInfo(0);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        input_shape_ = tensor_info.GetShape();
        std::cout << "    Shape size: " << input_shape_.size() << std::endl;
        
        if (input_shape_.size() >= 4) {
            in_height_ = static_cast<int>(input_shape_[2]);
            in_width_ = static_cast<int>(input_shape_[3]);
        }
        
        std::cout << "[6] Getting output names..." << std::endl;
        size_t output_count = session_->GetOutputCount();
        for (size_t i = 0; i < output_count; ++i) {
            auto out_allocated = session_->GetOutputNameAllocated(i, *allocator_);
            output_name_.push_back(std::string(out_allocated.get()));
        }
        std::cout << "[7] Done!" << std::endl;
    }

protected:
    Ort::Env env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::AllocatorWithDefaultOptions> allocator_;
    std::string model_path_;
    std::string model_name_;
    std::string input_name_;
    std::vector<std::string> output_name_;
    int in_height_{0}, in_width_{0};
    std::vector<int64_t> input_shape_;
};

class ClassifyInference : public ModelInference {
public:
    ClassifyInference(const std::string& model_path, const std::string& label_path)
    : ModelInference(model_path, "ClassifyInference"), label_path_(label_path) {
        std::cout << "[8] ClassifyInference constructor done" << std::endl;
    }
private:
    std::string label_path_;
};

int main() {
    try {
        ClassifyInference model("models/mobilenetv2/mobilenetv2-7.onnx", "imagenet_classes.txt");
        std::cout << "SUCCESS!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
