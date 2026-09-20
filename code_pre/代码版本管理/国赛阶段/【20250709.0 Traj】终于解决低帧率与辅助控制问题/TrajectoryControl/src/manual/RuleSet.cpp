#include <regex>
#include <fstream>
#include "Yaml.hpp"
#include "RuleSet.h"

SSD::SimPoint3D parsePoint(const std::string& s) {
    std::regex regex("\\(([^,]+),([^,]+),([^\\)]+)\\)"); // 正则表达式匹配点类型
    std::smatch match;
    if (std::regex_search(s, match, regex)) return SSD::SimPoint3D(std::stof(match[1]), std::stof(match[2]), std::stof(match[3]));
    return SSD::SimPoint3D(0.0f, 0.0f, 0.0f);
}

bool RuleSet::loadFromYaml(const std::string& yamlText) {
    Yaml::Node root;
    Yaml::Parse(root, yamlText);

    for (auto it = root.Begin(); it != root.End(); it++) {
        auto pair = *it;
        const std::string& keyStr = pair.first;
        Yaml::Node& node = pair.second;

        int id = std::stoi(keyStr);
        Rule rule;

        auto& whenNode = node["when"];
        rule.when.time = whenNode["time"].As<std::string>();
        rule.when.point = parsePoint(whenNode["point"].As<std::string>());

        auto& thenNode = node["then"];
        rule.then.stop = thenNode["stop"].As<std::string>();
        rule.then.point = parsePoint(thenNode["point"].As<std::string>());
        rule.then.set = thenNode["set"].As<std::string>();

        rules[id] = rule;
    }

    return true;
}

// 从 YAML 文件加载规则
bool RuleSet::loadFromYamlFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return false;
    }

    std::string yamlText((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()); // 读取文件内容到字符串
    return loadFromYaml(yamlText);
}

void RuleSet::print() const {
    for (const auto& pair : rules) {
        int id = pair.first;
        const Rule& rule = pair.second;

        auto [wx, wy, wz] = rule.when.point;
        auto [tx, ty, tz] = rule.then.point;

        std::cout << "规则 ID: " << id << "\n";
        std::cout << "  When:\n";
        std::cout << "    time:  " << rule.when.time << "\n";
        std::cout << "    point: (" << wx << ", " << wy << ", " << wz << ")\n";
        std::cout << "  Then:\n";
        std::cout << "    stop:  " << rule.then.stop << "\n";
        std::cout << "    point: (" << tx << ", " << ty << ", " << tz << ")\n";
        std::cout << "    set:   " << rule.then.set << "\n";
        std::cout << std::string(40, '-') << "\n";
    }
}