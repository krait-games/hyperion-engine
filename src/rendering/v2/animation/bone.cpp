#include "bone.h"

namespace hyperion::v2 {

Keyframe::Keyframe()
    : m_time(0.0f)
{
}

Keyframe::Keyframe(const Keyframe &other)
    : m_time(other.m_time),
      m_transform(other.m_transform)
{
}

Keyframe::Keyframe(float time, const Transform &transform)
    : m_time(time),
      m_transform(transform)
{
}

Bone::Bone(const char *tag)
    : Node(Type::BONE, tag, nullptr, Transform()),
      m_skeleton(nullptr)
{
}

Bone::~Bone() = default;

Vector3 Bone::GetOffsetTranslation() const
{
    return m_local_transform.GetTranslation() - m_binding_transform.GetTranslation();
}

Quaternion Bone::GetOffsetRotation() const
{
    return m_local_transform.GetRotation() * Quaternion(m_binding_transform.GetRotation()).Invert();
}

void Bone::SetKeyframe(const Keyframe &keyframe)
{
    m_keyframe = keyframe;

    m_pose_transform = m_keyframe.GetTransform();

    UpdateBoneTransform();
}

void Bone::ClearPose()
{
    m_pose_transform = Transform();

    UpdateBoneTransform();

    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->ClearPose();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::StoreBindingPose()
{
    m_inv_binding_translation = m_world_bone_translation * -1.0f;
    m_inv_binding_rotation = Quaternion(m_world_bone_rotation).Invert();

    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->StoreBindingPose();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::SetToBindingPose()
{
    m_local_transform = m_binding_transform;
    m_pose_transform = m_binding_transform;

    UpdateBoneTransform();

    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->SetToBindingPose();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::CalculateBoneTranslation()
{
    m_world_bone_translation = m_binding_transform.GetTranslation();

    if (m_parent_node != nullptr && m_parent_node->GetType() == Type::BONE) {
        const auto *parent_bone = static_cast<Bone *>(m_parent_node);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        m_world_bone_translation = parent_bone->m_world_bone_rotation * m_binding_transform.GetTranslation();
        m_world_bone_translation += parent_bone->m_world_bone_translation;
    }

    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->CalculateBoneTranslation();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::CalculateBoneRotation()
{
    m_world_bone_rotation = m_binding_transform.GetRotation();

    if (m_parent_node != nullptr && m_parent_node->GetType() == Type::BONE) {
        const auto *parent_bone = static_cast<Bone *>(m_parent_node);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        m_world_bone_rotation = parent_bone->m_world_bone_rotation * m_binding_transform.GetRotation();
    }

    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->CalculateBoneRotation();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::UpdateBoneTransform()
{
    m_bone_matrix = Matrix4::Translation(m_world_bone_translation * -1.0f);
    m_bone_matrix *= Matrix4::Rotation(m_world_bone_rotation * m_pose_transform.GetRotation() * GetOffsetRotation() * m_inv_binding_rotation);
    m_bone_matrix *= Matrix4::Translation(m_world_bone_translation + m_pose_transform.GetTranslation() + GetOffsetTranslation());

    if (m_parent_node != nullptr) {
        if (m_parent_node->GetType() == Type::BONE) {
            m_bone_matrix *= static_cast<Bone *>(m_parent_node)->GetBoneMatrix();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        }
    }

    if (m_skeleton != nullptr) {
        m_skeleton->SetShaderDataState(ShaderDataState::DIRTY);
    }
}

void Bone::SetSkeleton(Skeleton *skeleton)
{
    m_skeleton = skeleton;
    
    for (auto &child : m_child_nodes) {
        if (child == nullptr || child->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.get())->SetSkeleton(skeleton);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}


} // namespace hyperion::v2