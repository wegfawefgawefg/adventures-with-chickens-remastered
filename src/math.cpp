#include "math.hpp"

#include <cmath>
#include <cstdlib>

IVec2 operator-(IVec2 value) {
    return {-value.x, -value.y};
}

IVec2 operator+(IVec2 left, IVec2 right) {
    return {left.x + right.x, left.y + right.y};
}

IVec2 operator-(IVec2 left, IVec2 right) {
    return {left.x - right.x, left.y - right.y};
}

IVec2 operator*(IVec2 value, int scalar) {
    return {value.x * scalar, value.y * scalar};
}

IVec2 operator*(int scalar, IVec2 value) {
    return value * scalar;
}

IVec2 operator/(IVec2 value, int scalar) {
    return {value.x / scalar, value.y / scalar};
}

IVec2& operator+=(IVec2& left, IVec2 right) {
    left = left + right;
    return left;
}

IVec2& operator-=(IVec2& left, IVec2 right) {
    left = left - right;
    return left;
}

IVec2& operator*=(IVec2& value, int scalar) {
    value = value * scalar;
    return value;
}

IVec2& operator/=(IVec2& value, int scalar) {
    value = value / scalar;
    return value;
}

int length_squared(IVec2 value) {
    return value.x * value.x + value.y * value.y;
}

float length(IVec2 value) {
    return std::hypot(static_cast<float>(value.x), static_cast<float>(value.y));
}

int manhattan_length(IVec2 value) {
    return std::abs(value.x) + std::abs(value.y);
}
