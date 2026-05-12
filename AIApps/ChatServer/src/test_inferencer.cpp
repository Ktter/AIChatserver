/**
 * Inferencer 模块综合测试
 * 
 * 测试内容：
 * 1. ClassifyInference (MobileNetV2) - 图像分类
 * 2. DetectionAndSegmentInference (FasterRCNN) - 目标检测
 * 3. 边界情况 - 空图、无效路径
 */

#include <iostream>
#include <vector>
#include <string>
#include <any>
#include <cassert>
#include <opencv2/opencv.hpp>
#include "../include/AIUtil/Inferencer.h"

// ========== 辅助工具 ==========

// 打印检测结果
void printDetectResults(const std::vector<DetectResult>& dets, 
    const DetectionAndSegmentInference& model) {
    std::cout << "  检测到 " << dets.size() << " 个目标" << std::endl;
    for (size_t i = 0; i < dets.size() && i < 10; ++i) {
        const auto& det = dets[i];
        const std::string label_name = model.GetLabelName(det.label);
        std::cout << "    [" << i << "] Label=" << label_name 
                  << " Box=[" << det.box.x << "," << det.box.y 
                  << "," << det.box.width << "," << det.box.height 
                  << "] Score=" << det.score << std::endl;
    }
    if (dets.size() > 10) {
        std::cout << "    ... 还有 " << (dets.size() - 10) << " 个结果" << std::endl;
    }
}

// ========== 测试用例 ==========

// Test 1: 分类模型 - 正常推理
void test_classify_normal() {
    std::cout << "\n========== Test 1: 分类模型正常推理 ==========" << std::endl;
    try {
        ClassifyInference model(
            "models/mobilenetv2/mobilenetv2-7.onnx",
            "imagenet_classes.txt"
        );
        
        auto result = model.PredictFromFile("images/image1.jpg");
        std::string label = std::any_cast<std::string>(result);
        
        std::cout << "  [PASS] 预测结果: " << label << std::endl;
        assert(!label.empty());
        assert(label != "Unknown");
    } catch (const std::exception& e) {
        std::cerr << "  [FAIL] " << e.what() << std::endl;
        throw;
    }
}

// Test 2: 分类模型 - 多张图片
void test_classify_multiple_images() {
    std::cout << "\n========== Test 2: 分类模型多张图片 ==========" << std::endl;
    ClassifyInference model(
        "models/mobilenetv2/mobilenetv2-7.onnx",
        "imagenet_classes.txt"
    );
    
    std::vector<std::string> images = {
        "images/image1.jpg",
        "images/image2.jpg", 
        "images/image3.jpg"
    };
    
    for (const auto& img_path : images) {
        try {
            auto result = model.PredictFromFile(img_path);
            std::string label = std::any_cast<std::string>(result);
            std::cout << "  [PASS] " << img_path << " -> " << label << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "  [FAIL] " << img_path << ": " << e.what() << std::endl;
        }
    }
}

// Test 3: 分类模型 - 空图像异常
void test_classify_empty_image() {
    std::cout << "\n========== Test 3: 分类模型空图像异常 ==========" << std::endl;
    ClassifyInference model(
        "models/mobilenetv2/mobilenetv2-7.onnx",
        "imagenet_classes.txt"
    );
    
    cv::Mat empty_img;
    try {
        model.Predict(empty_img);
        std::cerr << "  [FAIL] 应该抛出异常但没有" << std::endl;
        assert(false);
    } catch (const std::runtime_error& e) {
        std::cout << "  [PASS] 正确捕获异常: " << e.what() << std::endl;
    }
}

// Test 4: 分类模型 - 无效路径异常
void test_classify_invalid_path() {
    std::cout << "\n========== Test 4: 分类模型无效路径异常 ==========" << std::endl;
    ClassifyInference model(
        "models/mobilenetv2/mobilenetv2-7.onnx",
        "imagenet_classes.txt"
    );
    
    try {
        model.PredictFromFile("/nonexistent/path.jpg");
        std::cerr << "  [FAIL] 应该抛出异常但没有" << std::endl;
        assert(false);
    } catch (const std::runtime_error& e) {
        std::cout << "  [PASS] 正确捕获异常: " << e.what() << std::endl;
    }
}

// Test 5: 检测模型 - 正常推理
void test_detection_normal() {
    std::cout << "\n========== Test 5: 检测模型正常推理 ==========" << std::endl;
    try {
        DetectionAndSegmentInference model("models/fast_rcnn/FasterRCNN-12.onnx", "models/fast_rcnn/coco_classes.txt");
        
        auto result = model.PredictFromFile("/home/wx/Documents/my_workspace/CppAIService/models/fast_rcnn/demo.jpg");
        auto dets = std::any_cast<std::vector<DetectResult>>(result);
        
        printDetectResults(dets, model);
        
        // 基本校验：坐标在合理范围内
        for (const auto& det : dets) {
            assert(det.box.x >= 0);
            assert(det.box.y >= 0);
            assert(det.box.width > 0);
            assert(det.box.height > 0);
            assert(det.score > 0.0f && det.score <= 1.0f);
        }
        
        std::cout << "  [PASS] 检测模型推理成功" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "  [FAIL] " << e.what() << std::endl;
        throw;
    }
}

// Test 6: 检测模型 - 空图像异常
void test_detection_empty_image() {
    std::cout << "\n========== Test 6: 检测模型空图像异常 ==========" << std::endl;
    DetectionAndSegmentInference model("models/fast_rcnn/FasterRCNN-12.onnx", "models/fast_rcnn/coco_classes.txt");
    
    cv::Mat empty_img;
    try {
        model.Predict(empty_img);
        std::cerr << "  [FAIL] 应该抛出异常但没有" << std::endl;
        assert(false);
    } catch (const std::runtime_error& e) {
        std::cout << "  [PASS] 正确捕获异常: " << e.what() << std::endl;
    }
}

// Test 7: 检测模型 - 无效路径异常
void test_detection_invalid_path() {
    std::cout << "\n========== Test 7: 检测模型无效路径异常 ==========" << std::endl;
    DetectionAndSegmentInference model("models/fast_rcnn/FasterRCNN-12.onnx", "models/fast_rcnn/coco_classes.txt");
    
    try {
        model.PredictFromFile("/nonexistent/path.jpg");
        std::cerr << "  [FAIL] 应该抛出异常但没有" << std::endl;
        assert(false);
    } catch (const std::runtime_error& e) {
        std::cout << "  [PASS] 正确捕获异常: " << e.what() << std::endl;
    }
}

// Test 8: 模型元信息
void test_model_metadata() {
    std::cout << "\n========== Test 8: 模型元信息 ==========" << std::endl;
    
    try {
        std::cout << "  正在加载分类模型..." << std::endl;
        ClassifyInference cls_model(
            "models/mobilenetv2/mobilenetv2-7.onnx",
            "imagenet_classes.txt"
        );
        std::cout << "  分类模型名称: " << cls_model.GetModelName() << std::endl;
        std::cout << "  分类模型路径: " << cls_model.GetModelMetaInfo() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "  [FAIL] 分类模型加载失败: " << e.what() << std::endl;
        throw;
    }
    
    try {
        std::cout << "  正在加载检测模型..." << std::endl;
        DetectionAndSegmentInference det_model("models/fast_rcnn/FasterRCNN-12.onnx", "models/fast_rcnn/coco_classes.txt");
        std::cout << "  检测模型名称: " << det_model.GetModelName() << std::endl;
        std::cout << "  检测模型路径: " << det_model.GetModelMetaInfo() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "  [FAIL] 检测模型加载失败: " << e.what() << std::endl;
        throw;
    }
    
    std::cout << "  [PASS] 元信息获取成功" << std::endl;
}

// ========== 主函数 ==========

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Inferencer 模块综合测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        test_model_metadata();
        test_classify_normal();
        test_classify_multiple_images();
        test_classify_empty_image();
        test_classify_invalid_path();
        test_detection_normal();
        test_detection_empty_image();
        test_detection_invalid_path();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "  所有测试通过！" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "  测试失败: " << e.what() << std::endl;
        std::cerr << "========================================" << std::endl;
        return 1;
    }
}
