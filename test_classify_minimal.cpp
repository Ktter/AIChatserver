#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>

void LoadLabels(const std::string& label_path, std::vector<std::string>& labels) {
    std::ifstream infile(label_path);
    if (!infile.is_open()) {
        throw std::runtime_error("标签文件打开失败: " + label_path);
    }
    std::string label;
    while(std::getline(infile, label)) {
        if (!label.empty()) {
            labels.push_back(label);
        }
    }
    std::cout << "Loaded " << labels.size() << " labels" << std::endl;
}

int main() {
    try {
        std::vector<std::string> labels;
        LoadLabels("imagenet_classes.txt", labels);
        std::cout << "First label: " << labels[0] << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
