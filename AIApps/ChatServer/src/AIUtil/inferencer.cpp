#include "../include/AIUtil/Inferencer.h"

ModelInference::ModelInference(const std::string& model_path,
    const std::string& model_name, const std::string& label_path)
: env_(ORT_LOGGING_LEVEL_WARNING, model_name.c_str())
, model_name_(model_name), model_path_(model_path), label_path_(label_path)
 {  
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    session_ = std::make_unique<Ort::Session>(env_, model_path_.c_str(), session_options);
    allocator_ = std::make_unique<Ort::AllocatorWithDefaultOptions>();
    auto in_allocated = session_->GetInputNameAllocated(0, *allocator_);
    input_name_ = std::string(in_allocated.get());
    // 下面这段代码拿到的 typeinfo 是临时的，链式调用后就被销毁，后续getshape会导致放问已经释放的内存
    // auto in_tensor_info = session_->GetInputTypeInfo(0)
    //                   .GetTensorTypeAndShapeInfo();
    auto type_info = session_->GetInputTypeInfo(0);
    auto in_tensor_info = type_info.GetTensorTypeAndShapeInfo();
    input_shape_ = in_tensor_info.GetShape();
    if (input_shape_.size() >= 4) {
        in_height_ = static_cast<int>(input_shape_[2]);
        in_width_ = static_cast<int>(input_shape_[3]);
    }
    std::cout << "[Inference 调试]"
              << " input_shape.size: " << input_shape_.size()
              << " 有 " << session_->GetOutputCount() << "个输出"
              << std::endl; 

    // 支持多输出，如 分割模型 box score label
    size_t output_count = session_->GetOutputCount();
    for (size_t i = 0; i < output_count; ++i) {
        auto out_allocated = session_->GetOutputNameAllocated(i, *allocator_);
        output_name_.push_back(std::string(out_allocated.get()));
    }
    LoadLabels(label_path);
}

std::string ModelInference::GetModelMetaInfo() const {
    // 暂时过渡，只返回模型的路径, 后续定义结构提返回
    return model_path_;
}

std::string ModelInference::GetModelName() const {
    return model_name_;
}

void ModelInference::LoadLabels(const std::string& label_path) {
    std::ifstream infile(label_path);
    if (!infile.is_open()) {
        throw std::runtime_error("标签文件打开失败, 请检查路径: " + label_path);
    }
    std::string label;
    while(std::getline(infile, label)) {
        if (!label.empty()) {
            labels_.push_back(label);
        }
    }
    if (labels_.empty()) {
        throw std::runtime_error("标签文件读取为空，请检查: " + label_path);
    }
}

std::any ClassifyInference::PredictFromFile(const std::string& img_path) {
    cv::Mat img = cv::imread(img_path);
    if (img.empty()) {
        throw std::runtime_error("Failed to decode image from path: " + img_path);
    }
    return Predict(img);
} 

std::any ClassifyInference::Predict(const cv::Mat& raw_img) {
    if (raw_img.empty()) {
        throw std::runtime_error("[ClassifyInference]Input image is empty.");
    }
    cv::Mat img;
    // 调整图片到固定 高宽, 像素值归一化到0-1
    cv::resize(raw_img, img, cv::Size(in_width_, in_height_));
    img.convertTo(img, CV_32F, 1.0 / 255.0);
    // NHWC -> NCHW
    cv::dnn::blobFromImage(img, img);
    std::vector<int64_t> dims = {1, 3, in_height_, in_width_};
    size_t input_tensor_size = 1 * 3 * in_height_ * in_width_;
    std::cout << "[ClassifyInference] Input prams, height: " << in_height_
              << " width: " << in_width_ << std::endl;
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
        OrtAllocatorType::OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, img.ptr<float>(), input_tensor_size,
        dims.data(), dims.size());
    
    const char* input_name[] = {input_name_.c_str()};
    if (output_name_.size() != 1) {
        throw std::runtime_error("[ClassifyInference] Output name number invalid.");
    }
    const char* output_name[] = {output_name_[0].c_str()};
    Ort::RunOptions run_options;
    // 1.x+ runtime 不需要加锁，内部线程安全
    auto output_tensors = session_->Run(
        run_options,
        input_name, &input_tensor, 1,
        output_name, 1);
    float* output_data = output_tensors.front().GetTensorMutableData<float>();
    int nums_class = labels_.empty() ? 1000 : static_cast<int>(labels_.size());
    int pred_class = std::max_element(output_data, output_data + nums_class)
                     - output_data;
    if (pred_class >= 0 &&  pred_class < (int)labels_.size()) {
        return labels_[pred_class];
    }
    return "Unknown";
}

std::any DetectionAndSegmentInference::Predict(const cv::Mat& raw_img) {
    if (raw_img.empty()) {
        throw std::runtime_error(
            "[DetectionAndSegmentInference] Raw img is empty.");
    }
    //预处理
    int h = static_cast<int>(raw_img.rows);
    int w = static_cast<int>(raw_img.cols);
    double ratio = 800.0 / std::min(h, w);
    int new_h = static_cast<int>(ratio * h);
    int new_w = static_cast<int>(ratio * w);
    if (new_h > 1333 || new_w > 1333) {
        ratio = 1333.0 / std::min(h, w);
        new_h = static_cast<int>(ratio * h);
        new_w = static_cast<int>(ratio * w);
    }
    cv::Mat img;
    // 转换 宽 高 到 800以上
    cv::resize(raw_img, img, cv::Size(new_w, new_h), 0, 0, cv::INTER_LINEAR);
    img.convertTo(img, CV_32F);
    std::vector<cv::Mat> channels(3);
    cv::split(img, channels);
    // 减均值 102.9801, 115.9465, 122.7717 B G R
    channels[0] -= 102.9801f;
    channels[1] -= 115.9465f;
    channels[2] -= 122.7717f;
    // 填充
    int padded_h = ((new_h + 31) / 32) * 32;
    int padded_w = ((new_w + 31) / 32) * 32;
    std::vector<float> input_data(3 * padded_h * padded_w, 0.0f);
    for (int c = 0; c < 3; ++c) {
        for (int h = 0; h < new_h; ++h) {
            memcpy(
                input_data.data() + c * padded_h * padded_w + h * padded_w,
                channels[c].ptr<float>(h), new_w * sizeof(float)
            );
        }
    }

    std::vector<int64_t> dims = {3, padded_h, padded_w};
    std::cout << "[DetectionAndSegmentInference] Input padding prams, height: " 
              << padded_h << " width: " << padded_w << std::endl;
    
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
        OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_data.data(), input_data.size(),
        dims.data(), dims.size());
    
    const char* input_name[] = {input_name_.c_str()};  
    // 需要将string 转为 const char*  
    std::vector<const char*> output_name;
    for (const auto& name: output_name_) {
        output_name.push_back(name.c_str()); 
    }
    Ort::RunOptions run_options;
    auto output_tensor = session_->Run(
        run_options,
        input_name, &input_tensor, 1,
        output_name.data(), output_name.size());
    
    std::vector<DetectResult> post_result = 
        PostProcess(output_tensor, cv::Size(w, h), ratio);
    
    std::cout << "[DetectionAndSegmentInference] PostPorcess has been done."
              << " Total boxes count: " << post_result.size() << std::endl;
    return post_result;
} 

std::vector<DetectResult> DetectionAndSegmentInference::PostProcess(
    const std::vector<Ort::Value>& outputs, const cv::Size& img_size,
    const double ratio, float conf_thresh, float nms_threash
) { 
    if (outputs.size() != 3) {
        throw std::runtime_error(
            "[DetectionAndSegmentInference] Faster RCNN output nums invalid:"
            + std::to_string(outputs.size()));
    }
    const float* boxes_data = outputs[0].GetTensorData<float>();
    const int64_t* labels_data = outputs[1].GetTensorData<int64_t>();
    const float* scores_data = outputs[2].GetTensorData<float>();
    // 获取检测框数量
    auto boxes_info = outputs[0].GetTensorTypeAndShapeInfo();
    auto boxes_shape = boxes_info.GetShape();
    int boxes_num = boxes_shape[0];

    std::vector<DetectResult> dets;
    for (int i = 0; i < boxes_num; ++i) {
        float score = scores_data[i];
        if (score < conf_thresh) continue;
        // 模型处理后的还原
        float x1 = boxes_data[i * 4 + 0] / ratio;
        float y1 = boxes_data[i * 4 + 1] / ratio;
        float x2 = boxes_data[i * 4 + 2] / ratio;
        float y2 = boxes_data[i * 4 + 3] / ratio;
        x1 = std::clamp(x1, 0.0f, (float)img_size.width - 1);
        y1 = std::clamp(y1, 0.0f, (float)img_size.height - 1);
        x2 = std::clamp(x2, 0.0f, (float)img_size.width - 1);
        y2 = std::clamp(y2, 0.0f, (float)img_size.height - 1);
        // 去除无用数据
        if (x1 > x2 || y1 > y2) continue;
        int label = static_cast<int>(labels_data[i]);
        dets.emplace_back(
            cv::Rect(cv::Point(x1, y1), cv::Point(x2, y2)),
            label, score
        );
    }
    return dets;
}

std::any DetectionAndSegmentInference::PredictFromFile(const std::string& img_path) {
    cv::Mat img = cv::imread(img_path);
    if (img.empty()) {
        throw std::runtime_error("Failed to decode image from path: " + img_path);
    }
    return Predict(img);
} 

std::string DetectionAndSegmentInference::GetLabelName(const size_t label) const {
    if (label < 0 || label > labels_.size()) {
        throw std::runtime_error(
            "[DetectionAndSegmentInference] Pass label params invalid.");
    }
    return labels_[label];
}




