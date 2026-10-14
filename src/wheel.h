#pragma once
#include <cmath>
#include <array>
#include <string>
namespace wheel {
constexpr double pi = 3.14159265358979323846;
// Clockwise from north. Neutral never commits a character.
inline int sector(double x, double y) {
    if (std::hypot(x, y) < 0.35) return -1;
    double angle = std::atan2(x, -y);
    return (int(std::floor(angle / (pi / 4) + 0.5)) + 8) % 8;
}
// Slots correspond to physical south/east/west/north controller buttons.
inline const std::array<std::string, 8> letters = {
    "abcd", "efgh", "ijkl", "mnop", "qrst", "uvwx", "yz.,", "!?'-"
};
inline const std::array<std::string, 8> symbols = {
    "1234", "5678", "90+-", "*/=:", ";()_", "[]{}", "@#$%", "&<>\\"
};
}
