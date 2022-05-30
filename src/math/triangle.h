#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "vertex.h"
#include "ray.h"
#include "transform.h"
#include "matrix4.h"

#include <array>

namespace hyperion {
class Triangle {
public:
    Triangle();
    Triangle(const Vertex &v0, const Vertex &v1, const Vertex &v2);
    Triangle(const Triangle &other);
    ~Triangle() = default;

    Triangle operator*(const Matrix4 &mat) const;
    Triangle &operator*=(const Matrix4 &mat);
    Triangle operator*(const Transform &transform) const;
    Triangle &operator*=(const Transform &transform);

    inline Vertex &operator[](int index) { return m_points[index]; }
    inline const Vertex &operator[](int index) const { return m_points[index]; }
    inline Vertex &GetPoint(int index) { return operator[](index); }
    inline const Vertex &GetPoint(int index) const { return operator[](index); }
    inline void SetPoint(int index, const Vertex &value) { m_points[index] = value; }

    inline Vector3 GetCenter() const
    {
        return (m_points[0].GetPosition() + m_points[1].GetPosition() + m_points[2].GetPosition()) / 3.0f;
    }

    Vertex &Closest(const Vector3 &vec);
    const Vertex &Closest(const Vector3 &vec) const;
    bool IntersectRay(const Ray &ray, RayTestResults &out) const;

private:
    std::array<Vertex, 3> m_points;
};
} // namespace hyperion

#endif
