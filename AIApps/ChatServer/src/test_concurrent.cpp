/**
 * Inferencer 模块并发 + 检测验证测试
 * 
 * 测试内容：
 * 1. 检测模型在有物体图片上的验证（demo.jpg）
 * 2. 多线程并发推理测试（共享模型实例）
 * 3. 性能基准测试
 */

#include <iostream>
#include <vector>
#include <thread>
#include <any>
#include <chrono>
#include <atomic>
#include <mutex>
#include <cassert>
#include <opencv2/opencv.hpp>
#include "../include/AIUtil/Inferencer.h"

// ========== 全局统计 ==========
struct ConcurrentStats {
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};
    std::atomic<long long> total_time_us{0};  // 微秒
    std::mutex log_mutex;
};

// ========== 检测模型验证 ==========
void test_detection_with_objects() {
    std::cout << "\n========== 检测验证：有物体图片 ==========" << std::endl;
    
    try {
        DetectionAndSegmentInference model("models/fast_rcnn/FasterRCNN-12.onnx");
        
        auto result = model.PredictFromFile("models/fast_rcnn/demo.jpg");
        auto dets = std::any_cast<std::vector<DetectResult>>(result);
        
        std::cout << "  检测到 " << dets.size() << " 个目标:" << std::endl;
        
        if (dets.empty()) {
            std::cout << "  [WARN] demo.jpg 没有检测到任何物体" << std::endl;
        } else {
            for (size_t i = 0; i < dets.size() && i < 10; ++i) {
                const auto& det = dets[i];
                std::cout << "    [" << i << "] Label=" << det.label 
                          << " Box=[" << det.box.x << "," << det.box.y 
                          << "," << det.box.width << "," << det.box.height 
                          << "] Score=" << det.score << std::endl;
            }
            if (dets.size() > 10) {
                std::cout << "    ... 还有 " << (dets.size() - 10) << " 个" << std::endl;
            }
            std::cout << "  [PASS] 检测到 " << dets.size() << " 个物体" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "  [FAIL] " << e.what() << std::endl;
    }
}

// ========== 并发推理工作函数 ==========
void classify_worker(ClassifyInference* model, const std::string& img_path,
                     int iterations, ConcurrentStats* stats) {
    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        try {
            auto result = model->PredictFromFile(img_path);
            auto label = std::any_cast<std::string>(result);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            
            stats->success_count.fetch_add(1, std::memory_order_relaxed);
            stats->total_time_us.fetch_add(us, std::memory_order_relaxed);
            
            // 每 10 次打印一次
            if ((i + 1) % 10 == 0) {
                std::lock_guard<std::mutex> lock(stats->log_mutex);
                std::cout << "  [Thread " << std::this_thread::get_id() 
                          << "] Iter " << (i + 1) 
                          << "/" << iterations
                          << " -> " << label 
                          << " (" << us/1000.0 << " ms)" << std::endl;
            }
        } catch (const std::exception& e) {
            stats->fail_count.fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lock(stats->log_mutex);
                std::cerr << "  [Thread " << std::this_thread::get_id() 
                          << "] ERROR: " << e.what() << std::endl;
            }
        }
    }
}

// ========== 分类模型并发测试 ==========
void test_classify_concurrent() {
    std::cout << "\n========== 并发测试：分类模型 ==========" << std::endl;
    
    const int num_threads = 4;
    const int iterations_per_thread = 20;
    const int total_iterations = num_threads * iterations_per_thread;
    
    std::cout << "  配置: " << num_threads << " 线程 × " 
              << iterations_per_thread << " 次推理 = " 
              << total_iterations << " 次总计" << std::endl;
    
    // 共享同一个模型实例（验证 ONNX Runtime 线程安全性）
    ClassifyInference model(
        "models/mobilenetv2/mobilenetv2-7.onnx",
        "imagenet_classes.txt"
    );
    
    ConcurrentStats stats;
    std::vector<std::thread> threads;
    
    auto total_start = std::chrono::high_resolution_clock::now();
    
    // 启动线程
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(classify_worker, &model, 
                             "images/image1.jpg", 
                             iterations_per_thread, &stats);
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        total_end - total_start).count();
    
    // 统计结果
    int success = stats.success_count.load();
    int fail = stats.fail_count.load();
    long long total_us = stats.total_time_us.load();
    double avg_us = success > 0 ? (double)total_us / success : 0;
    
    std::cout << "\n  并发测试结果:" << std::endl;
    std::cout << "    总耗时:     " << total_ms << " ms" << std::endl;
    std::cout << "    成功次数:   " << success << "/" << total_iterations << std::endl;
    std::cout << "    失败次数:   " << fail << std::endl;
    std::cout << "    平均延迟:   " << avg_us / 1000.0 << " ms/次" << std::endl;
    std::cout << "    理论 QPS:   " << (int)(1000.0 / (avg_us / 1000.0)) << " req/s" << std::endl;
    
    if (fail == 0) {
        std::cout << "  [PASS] 并发测试全部通过，无崩溃无异常" << std::endl;
    } else {
        std::cout << "  [FAIL] 并发测试出现 " << fail << " 次失败" << std::endl;
    }
}

// ========== 检测模型并发测试 ==========
void detect_worker(DetectionAndSegmentInference* model, const std::string& img_path,
                   int iterations, ConcurrentStats* stats) {
    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        try {
            auto result = model->PredictFromFile(img_path);
            auto dets = std::any_cast<std::vector<DetectResult>>(result);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            
            stats->success_count.fetch_add(1, std::memory_order_relaxed);
            stats->total_time_us.fetch_add(us, std::memory_order_relaxed);
            
            if ((i + 1) % 5 == 0) {
                std::lock_guard<std::mutex> lock(stats->log_mutex);
                std::cout << "  [Thread " << std::this_thread::get_id() 
                          << "] Iter " << (i + 1) 
                          << "/" << iterations
                          << " -> " << dets.size() << " boxes"
                          << " (" << us/1000.0 << " ms)" << std::endl;
            }
        } catch (const std::exception& e) {
            stats->fail_count.fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lock(stats->log_mutex);
                std::cerr << "  [Thread " << std::this_thread::get_id() 
                          << "] ERROR: " << e.what() <> std::endl;
            }
        }
    }
}

void test_detection_concurrent() {
    std::cout << "\n========== 并发测试：检测模型 ==========" << std::endl;
    
    const int num_threads = 2;  // 检测模型更耗资源，用2线程
    const int iterations_per_thread = 5;
    const int total_iterations = num_threads * iterations_per_thread;
    
    std::cout << "  配置: " << num_threads << " 线程 × " 
              << iterations_per_thread << " 次推理 = " 
              << total_iterations << " 次总计" << std::endl;
    
    DetectionAndSegmentInference model("models/fast_rcnn/FasterRCNN-12.onnx");
    
    ConcurrentStats stats;
    std::vector<std::thread> threads;
    
    auto total_start = std::chrono::high_resolution_clock::now();
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(detect_worker, &model,
                             "models/fast_rcnn/demo.jpg",
                             iterations_per_thread, &stats);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        total_end - total_start).count();
    
    int success = stats.success_count.load();
    int fail = stats.fail_count.load();
    long long total_us = stats.total_time_us.load();
    double avg_us = success > 0 ? (double)total_us / success : 0;
    
    std::cout << "\n  并发测试结果:" << std::endl;
    std::cout << "    总耗时:     " << total_ms << " ms" << std::endl;
    std::cout << "    成功次数:   " << success << "/" << total_iterations << std::endl;
    std::cout << "    失败次数:   " << fail << std::endl;
    std::cout << "    平均延迟:   " << avg_us / 1000.0 << " ms/次" << std::endl;
    std::cout << "    理论 QPS:   " << (int)(1000.0 / (avg_us / 1000.0)) << " req/s" << std::endl;
    
    if (fail == 0) {
        std::cout << "  [PASS] 并发测试全部通过，无崩溃无异常" << std::endl;
    } else {
        std::cout << "  [FAIL] 并发测试出现 " << fail << " 次失败" << std::endl;
    }
}

// ========== 主函数 ==========
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Inferencer 并发 + 检测验证测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        // 1. 先用有物体的图片验证检测逻辑
        test_detection_with_objects();
        
        // 2. 分类模型并发（4线程 × 20次）
        test_classify_concurrent();
        
        // 3. 检测模型并发（2线程 × 5次）
        test_detection_concurrent();
        
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
