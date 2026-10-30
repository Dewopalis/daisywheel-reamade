#pragma once
#include <cmath>
#include <array>
#include <string>
#include <utility>
#include <algorithm>
namespace wheel {
constexpr double pi = 3.14159265358979323846;
// Clockwise from north. Neutral never commits a character.
inline std::pair<double,double> applyDeadzone(double x,double y,double deadzone) {
    const double magnitude=std::hypot(x,y);
    deadzone=std::clamp(deadzone,0.0,0.95);
    if(magnitude<=deadzone||magnitude==0.0)return {0.0,0.0};
    const double scaled=std::min(1.0,(magnitude-deadzone)/(1.0-deadzone));
    return {x/magnitude*scaled,y/magnitude*scaled};
}
inline int sector(double x, double y,double deadzone=0.35) {
    if (std::hypot(x, y) < deadzone) return -1;
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
// Each entry is a complete UTF-8 character string, never an indexed byte.
// Extended Latin characters and common international punctuation/currency.
inline const std::array<std::array<const char *, 4>, 8> extendedLower = {{
    {{"â", "ê", "á", "à"}}, {{"è", "ë", "é", "ï"}},
    {{"í", "ì", "î", "ó"}}, {{"ò", "€", "£", "¥"}},
    {{"¢", "§", "¶", "°"}}, {{"…", "—", "–", "‘"}},
    {{"’", "“", "”", "‹"}}, {{"›", "¿", "¡", "×"}}
}};
inline const std::array<std::array<const char *, 4>, 8> extendedUpper = {{
    {{"Â", "Ê", "Á", "À"}}, {{"È", "Ë", "É", "Ï"}},
    {{"Í", "Ì", "Î", "Ó"}}, {{"Ò", "€", "£", "¥"}},
    {{"¢", "§", "¶", "°"}}, {{"…", "—", "–", "‘"}},
    {{"’", "“", "”", "‹"}}, {{"›", "¿", "¡", "×"}}
}};
}
