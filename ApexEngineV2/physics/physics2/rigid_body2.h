#ifndef APEX_PHYSICS_RIGID_BODY_H
#define APEX_PHYSICS_RIGID_BODY_H

#include "physics_shape.h"
#include "physics_material.h"
#include "../../math/vector3.h"
#include "../../math/matrix4.h"
#include "../../math/quaternion.h"

#include <memory>

namespace apex {
namespace physics {
class Rigidbody {
public:
    Rigidbody(std::shared_ptr<PhysicsShape> shape, PhysicsMaterial material);

    inline std::shared_ptr<PhysicsShape> GetPhysicsShape() const { return m_shape; }

    inline const PhysicsMaterial &GetPhysicsMaterial() const { return m_material; }
    inline PhysicsMaterial &GetPhysicsMaterial() { return m_material; }
    inline void SetPhysicsMaterial(const PhysicsMaterial &material) { m_material = material; }

    inline bool IsAwake() const { return m_awake; }
    inline void SetAwake(bool awake = true) { m_awake = awake; if (!awake) { m_velocity = 0; m_rotation = 0; } }

    inline void SetInertiaTensor(const Matrix3 &inertia_tensor) { m_inv_inertia_tensor = inertia_tensor; m_inv_inertia_tensor.Invert(); }
    inline const Matrix3 &GetInverseInertiaTensor() const { return m_inv_inertia_tensor; }
    inline void SetInverseInertiaTensor(const Matrix3 &inv_inertia_tensor) { m_inv_inertia_tensor = inv_inertia_tensor; }
    inline const Matrix3 &GetInverseInertiaTensorWorld() const { return m_inv_inertia_tensor_world; }

    inline bool IsStatic() const { return m_material.GetInverseMass() == 0.0; }

    inline const Vector3 &GetVelocity() const { return m_velocity; }
    inline void SetVelocity(const Vector3 &velocity) { m_velocity = velocity; }
    inline void AddVelocity(const Vector3 &velocity) { m_velocity += velocity; }
    inline const Vector3 &GetAcceleration() const { return m_acceleration; }
    inline void SetAcceleration(const Vector3 &acceleration) { m_acceleration = acceleration; }
    inline const Vector3 &GetLastAcceleration() const { return m_last_acceleration; }
    inline const Vector3 &GetPosition() const { return m_position; }
    inline void SetPosition(const Vector3 &position) { m_position = position; }
    inline const Vector3 &GetRotation() const { return m_rotation; }
    inline void SetRotation(const Vector3 &rotation) { m_rotation = rotation; }
    inline void AddRotation(const Vector3 &rotation) { m_rotation += rotation; }
    inline const Quaternion &GetOrientation() const { return m_orientation; }
    inline void SetOrientation(const Quaternion &orientation) { m_orientation = orientation; }

    inline void ApplyForce(const Vector3 &force) { m_force_accum += force; m_awake = true; }
    inline void ApplyTorque(const Vector3 &torque) { m_torque_accum += torque; m_awake = true; }

    // updates the transform of the PhysicsShape within this object
    void UpdateTransform();
    // perform physics calculations on this rigidbody
    void Integrate(double dt);

private:
    std::shared_ptr<PhysicsShape> m_shape;
    PhysicsMaterial m_material;
    bool m_awake;
    Matrix4 m_transform;
    Vector3 m_velocity;
    Vector3 m_acceleration;
    Vector3 m_last_acceleration;
    Vector3 m_position;
    Vector3 m_rotation;
    Quaternion m_orientation;
    Vector3 m_force_accum;
    Vector3 m_torque_accum;
    Matrix3 m_inv_inertia_tensor;
    Matrix3 m_inv_inertia_tensor_world;
};
} // namespace physics
} // namespace apex

#endif