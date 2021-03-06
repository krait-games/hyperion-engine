#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "Vertex.hpp"
#include "Transform.hpp"
#include "Matrix4.hpp"

#include <array>

namespace hyperion {
class Triangle {
public:
    Triangle();
    Triangle(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2);
    Triangle(const Vertex &v0, const Vertex &v1, const Vertex &v2);
    Triangle(const Triangle &other);
    ~Triangle() = default;

    Vertex &operator[](int index) { return m_points[index]; }
    const Vertex &operator[](int index) const { return m_points[index]; }
    Vertex &GetPoint(int index) { return operator[](index); }
    const Vertex &GetPoint(int index) const { return operator[](index); }
    void SetPoint(int index, const Vertex &value) { m_points[index] = value; }

    Vector3 GetCenter() const
    {
        return (m_points[0].GetPosition() + m_points[1].GetPosition() + m_points[2].GetPosition()) / 3.0f;
    }

    Vertex &Closest(const Vector3 &vec);
    const Vertex &Closest(const Vector3 &vec) const;
    // bool IntersectRay(const Ray &ray, RayTestResults &out) const;

private:
    std::array<Vertex, 3> m_points;
};
} // namespace hyperion

#endif
