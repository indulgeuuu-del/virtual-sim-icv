#pragma once
#include <algorithm>
#define IS_IN(value, container) (std::find((container).begin(), (container).end(), (value)) != (container).end())
