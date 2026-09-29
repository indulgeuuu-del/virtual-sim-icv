#pragma once
// Minimal globals for compiling the real manual/manual.cpp without SimOne.
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "json.hpp"
#include "manual.h"

struct Logger {
    enum class Color { BrightCyan, BrightMagenta };
    Logger& operator()(Color) { text += '\n'; return *this; }
    template<class T> Logger& operator<<(const T& v) {
        std::ostringstream os; os << v; text += os.str(); return *this;
    }
    std::string text;
};
inline Logger globalLogger;
inline std::string strategyRoot; // replaces the fixed SimOne install directory
#define SIMONE_ADAS_DIR std::string(strategyRoot)
inline int caseIdx = 0;
