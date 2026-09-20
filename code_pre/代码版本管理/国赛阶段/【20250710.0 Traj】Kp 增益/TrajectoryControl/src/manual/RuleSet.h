#pragma once
#include <string>
#include "SSD/SimPoint3D.h"

struct TriggerCondition {
    std::string time;
    SSD::SimPoint3D point;
};

struct Action {
    std::string stop;
    SSD::SimPoint3D point;
    std::string set;
};

struct Rule {
    TriggerCondition when;
    Action then;
};

class RuleSet {
public:
    std::map<int, Rule> rules;

    bool loadFromYaml(const std::string& yamlText);
    bool loadFromYamlFile(const std::string& filename);
    void print() const;
};