#include <iostream>
#include <numeric>
#include <vector>

int main() {
    const std::vector<int> values{1, 2, 3};
    if (sizeof(void*) != 8 || std::accumulate(values.begin(), values.end(), 0) != 6) {
        return 1;
    }
    std::cout << "MSVC x64 standard-library smoke passed\n";
    return 0;
}
