#ifndef VECTOR4_H
#define VECTOR4_H
#include "../hash_code.h"
#include "../util.h"

#include <cmath>
#include <iostream>

namespace hyperion {

class Vector2;
class Vector3;
class Matrix4;

class Vector4 {
    friend std::ostream &operator<<(std::ostream &out, const Vector4 &vec);
public:
    union {
        struct { float x, y, z, w; };
        float values[4];
    };

    Vector4();
    Vector4(float x, float y, float z, float w);
    Vector4(float xyzw);
    explicit Vector4(const Vector2 &xy, float z, float w);
    explicit Vector4(const Vector3 &xyz, float w);
    Vector4(const Vector4 &other);

    inline float GetX() const { return x; }
    inline float &GetX() { return x; }
    inline Vector4 &SetX(float x) { this->x = x; return *this; }
    inline float GetY() const { return y; }
    inline float &GetY() { return y; }
    inline Vector4 &SetY(float y) { this->y = y; return *this; }
    inline float GetZ() const { return z; }
    inline float &GetZ() { return z; }
    inline Vector4 &SetZ(float z) { this->z = z; return *this; }
    inline float GetW() const { return w; }
    inline float &GetW() { return w; }
    inline Vector4 &SetW(float w) { this->w = w; return *this; }
    
    constexpr inline float operator[](size_t index) const
        { return values[index]; }

    constexpr inline float &operator[](size_t index)
        { return values[index]; }

    Vector4 &operator=(const Vector4 &other);
    Vector4 operator+(const Vector4 &other) const;
    Vector4 &operator+=(const Vector4 &other);
    Vector4 operator-(const Vector4 &other) const;
    Vector4 &operator-=(const Vector4 &other);
    Vector4 operator*(const Vector4 &other) const;
    Vector4 &operator*=(const Vector4 &other);
    Vector4 operator*(const Matrix4 &mat) const;
    Vector4 &operator*=(const Matrix4 &mat);
    Vector4 operator/(const Vector4 &other) const;
    Vector4 &operator/=(const Vector4 &other);
    bool operator==(const Vector4 &other) const;
    bool operator!=(const Vector4 &other) const;
    inline Vector4 operator-() const { return operator*(-1.0f); }

    constexpr inline float LengthSquared() const { return x * x + y * y + z * z + w * w; }
    inline float Length() const { return sqrt(LengthSquared()); }

    float DistanceSquared(const Vector4 &other) const;
    float Distance(const Vector4 &other) const;
    
    Vector4 Normalized() const;
    Vector4 &Normalize();
    Vector4 &Rotate(const Vector3 &axis, float radians);
    Vector4 &Lerp(const Vector4 &to, float amt);
    float Dot(const Vector4 &other) const;

    static Vector4 Abs(const Vector4 &);
    static Vector4 Round(const Vector4 &);
    static Vector4 Clamp(const Vector4 &, float min, float max);
    static Vector4 Min(const Vector4 &a, const Vector4 &b);
    static Vector4 Max(const Vector4 &a, const Vector4 &b);

    static Vector4 Zero();
    static Vector4 One();
    static Vector4 UnitX();
    static Vector4 UnitY();
    static Vector4 UnitZ();
    static Vector4 UnitW();

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);
        hc.Add(z);
        hc.Add(w);

        return hc;
    }
};

static_assert(sizeof(Vector4) == sizeof(float) * 4, "sizeof(Vector4) must be equal to sizeof(float) * 4");

} // namespace hyperion

#endif
