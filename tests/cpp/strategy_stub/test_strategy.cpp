// Loads .stg files through the real loadStrategyPoints (manual/manual.cpp).
// The runner copies the repository's m_strategy/41.stg beside this binary.
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "define.h"

namespace {
struct Case { const char* name; const char* content; const char* diagnostic; };

// Every case must be rejected as a whole, leave the output untouched and say why.
const Case rejects[] = {
    {"malformed_json", R"({"strategy": [[0, 1, 2, -1, -1, -1, -1],)", "JSON"},
    {"missing_strategy", R"({"points": []})", "strategy"},
    {"strategy_not_array", R"({"strategy": 5})", "strategy"},
    {"empty_strategy", R"({"strategy": []})", "为空"},
    {"six_columns", R"({"strategy": [[0, 1, 2, -1, -1, -1]]})", "strategy[0]"},
    {"eight_columns", R"({"strategy": [[0, 1, 2, -1, -1, -1, -1, 9]]})", "strategy[0]"},
    {"row_not_array", R"({"strategy": [5]})", "strategy[0]"},
    {"string_coordinate", R"({"strategy": [[0, "1", 2, -1, -1, -1, -1]]})", "第 2 列 x"},
    {"null_kp", R"({"strategy": [[0, 1, 2, -1, -1, -1, null]]})", "第 7 列 kp"},
    {"bool_speed", R"({"strategy": [[0, 1, 2, -1, -1, true, -1]]})", "第 6 列 speed"},
    {"fractional_path_mode", R"({"strategy": [[0, 1, 2, 1.5, -1, -1, -1]]})", "第 4 列 pathMode"},
    {"unknown_path_mode", R"({"strategy": [[0, 1, 2, 0, -1, -1, -1]]})", "第 4 列 pathMode"},
    {"zero_stop_time", R"({"strategy": [[0, 1, 2, -1, 0, -1, -1]]})", "第 5 列 stopTime"},
    {"negative_speed", R"({"strategy": [[0, 1, 2, -1, -1, -5, -1]]})", "第 6 列 speed"},
    {"zero_kp", R"({"strategy": [[0, 1, 2, -1, -1, -1, 0]]})", "第 7 列 kp"},
    {"id_mismatch", R"({"strategy": [[0, 1, 2, -1, -1, -1, -1], [5, 3, 4, -1, -1, -1, -1]]})", "strategy[1] 第 1 列 id"},
    {"later_row_invalid", R"({"strategy": [[0, 1, 2, -1, -1, -1, -1], [1, 3, 4, -1, -1, -1]]})", "strategy[1]"},
};

void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
const std::string dir = "TrajectoryControl/m_strategy/";

// Returns the loader result; exceptions escape so a crash is reported as FAIL.
bool load(int idx, const char* content, std::vector<StrategyPoint>& out) {
    if (content) {
        std::filesystem::create_directories(dir);
        std::ofstream(dir + std::to_string(idx) + ".stg", std::ios::binary) << content;
    }
    globalLogger.text.clear();
    return loadStrategyPoints(idx, out);
}
StrategyPoint sentinel() {
    StrategyPoint p;
    p.Pos = SSD::SimPoint3D(-7, -7, 0);
    p.pathMode = 42; p.stopTime = p.speed = p.kp = 42;
    return p;
}
bool same(const StrategyPoint& a, double x, double y, int mode, double stop, double speed, double kp) {
    return a.Pos.x == x && a.Pos.y == y && a.Pos.z == 0 && a.pathMode == mode &&
           a.stopTime == stop && a.speed == speed && a.kp == kp;
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--list") {
        std::cout << "valid\nreal_41\nmissing_file\n";
        for (const auto& c : rejects) std::cout << c.name << '\n';
        return 0;
    }
    std::string name = argc == 2 ? argv[1] : "";
    try {
        std::vector<StrategyPoint> out;
        if (name == "valid") {
            out.push_back(sentinel()); // a successful load replaces, never appends
            require(load(900, R"({"strategy": [[0, 1.5, -2, -1, -1, -1, -1],
                                               [1, 3, 4, 1, 0.5, 6.5, 1.8]]})", out), "valid file rejected");
            require(out.size() == 2, "expected exactly 2 points, got " + std::to_string(out.size()));
            require(same(out[0], 1.5, -2, -1, -1, -1, -1) && same(out[1], 3, 4, 1, 0.5, 6.5, 1.8),
                    "fields parsed incorrectly");
        } else if (name == "real_41") {
            std::ifstream file(dir + "41.stg");
            require(bool(file), "repository 41.stg not copied");
            nlohmann::json expected = nlohmann::json::parse(file);
            const auto& rows = expected.at("strategy");
            require(load(41, nullptr, out), "repository 41.stg rejected: " + globalLogger.text);
            require(out.size() == rows.size(), "row count changed");
            for (size_t i = 0; i < rows.size(); ++i) {
                const auto& r = rows.at(i);
                require(same(out[i], r.at(1).get<double>(), r.at(2).get<double>(), r.at(3).get<int>(),
                             r.at(4).get<double>(), r.at(5).get<double>(), r.at(6).get<double>()),
                        "row " + std::to_string(i) + " differs");
            }
        } else if (name == "missing_file") {
            out.push_back(sentinel());
            require(!load(901, nullptr, out), "missing file accepted");
            require(out.size() == 1, "missing file changed output");
        } else {
            const Case* c = nullptr;
            for (const auto& r : rejects) if (name == r.name) c = &r;
            require(c != nullptr, "unknown test");
            out.push_back(sentinel());
            require(!load(902, c->content, out), "invalid file accepted");
            require(out.size() == 1 && same(out[0], -7, -7, 42, 42, 42, 42), "rejected file changed output");
            require(globalLogger.text.find("902.stg") != std::string::npos, "rejection did not name the file");
            require(globalLogger.text.find(c->diagnostic) != std::string::npos,
                    std::string("diagnostic lacks \"") + c->diagnostic + "\": " + globalLogger.text);
        }
        std::cout << "PASS " << name << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << name << ": " << e.what() << '\n';
        return 1;
    }
}
