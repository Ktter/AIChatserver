/**
 *  统一化所有的模型部署和推理部分内容
 *  提供通用的抽象接口
 * 
 */

#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <onnxruntime_cxx_api.h>

#include <string> 
#include <mutex>
#include <cstdint>
#include <fstream>
#include <memory>
#include <vector>
#include <any>
// #include <boost/any.hpp>

struct DetectResult {
    cv::Rect box;
    int label;
    float score;

    DetectResult() = default;
    DetectResult(const cv::Rect& b, const int l, const float s) :
    box(b), label(l), score(s) {}
};

class ModelInference {
public:
    explicit ModelInference(const std::string& model_path,
                            const std::string& model_name,
                            const std::string& label_path);
    virtual ~ModelInference() = default;
    virtual std::any Predict(const cv::Mat& raw_img) = 0;
    std::string GetModelMetaInfo() const;
    std::string GetModelName() const;

protected:
    // 一个进程一个模型env
    Ort::Env env_;
    // onnx runtime 1.x+是线程安全的
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::AllocatorWithDefaultOptions> allocator_;
    
    std::string model_path_;
    std::string model_name_;
    std::string input_name_;
    // 支持多输出
    std::vector<std::string> output_name_;
    int in_height_{0}, in_width_{0};
    std::vector<int64_t> input_shape_;

    std::string label_path_;
    std::vector<std::string> labels_;
    void LoadLabels(const std::string& label_path);
};

class ClassifyInference: public ModelInference {
public:
    explicit ClassifyInference(const std::string& model_path,
        const std::string& label_path)
    : ModelInference(model_path, "ClassifyInference", label_path) {}
    std::any Predict(const cv::Mat& raw_img) override;
    std::any PredictFromFile(const std::string& img_path);
};

class DetectionAndSegmentInference: public ModelInference {
public:
    explicit DetectionAndSegmentInference(const std::string& model_path, 
        const std::string& label_path)
    : ModelInference(model_path, "DetectionAndSegmentInference", label_path) {}
    std::vector<DetectResult> PostProcess(
        const std::vector<Ort::Value>& outputs, const cv::Size& img_size, 
        const double ratio, float conf_thresh = 0.5f, float nms_threash = 0.5f);
    std::any Predict(const cv::Mat& raw_img) override;
    std::any PredictFromFile(const std::string& img_path);
    std::string GetLabelName(const size_t label) const ;
};

