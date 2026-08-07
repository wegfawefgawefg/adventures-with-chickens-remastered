#pragma once

struct IVec2 {
    int x{};
    int y{};

    bool operator==(const IVec2&) const = default;
};

IVec2 operator-(IVec2 value);
IVec2 operator+(IVec2 left, IVec2 right);
IVec2 operator-(IVec2 left, IVec2 right);
IVec2 operator*(IVec2 value, int scalar);
IVec2 operator*(int scalar, IVec2 value);
IVec2 operator/(IVec2 value, int scalar);
IVec2& operator+=(IVec2& left, IVec2 right);
IVec2& operator-=(IVec2& left, IVec2 right);
IVec2& operator*=(IVec2& value, int scalar);
IVec2& operator/=(IVec2& value, int scalar);

int length_squared(IVec2 value);
float length(IVec2 value);
int manhattan_length(IVec2 value);
