#pragma once
namespace SSD {
struct SimPoint3D {
    double x = 0, y = 0, z = 0;
    SimPoint3D() = default;
    SimPoint3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
};
}
