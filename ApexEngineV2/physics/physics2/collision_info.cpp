#include "collision_info.h"

namespace apex {
namespace physics {
CollisionInfo::CollisionInfo()
    : m_contact_penetration(0.0)
{
    m_bodies = { nullptr };
}

CollisionInfo::CollisionInfo(const CollisionInfo &other)
    : m_contact_point(other.m_contact_point),
    m_contact_normal(other.m_contact_normal),
    m_contact_penetration(other.m_contact_penetration),
    m_bodies(other.m_bodies)
{
}
} // namespace physics
} // namespace apex