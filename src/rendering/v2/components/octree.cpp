#include "octree.h"
#include "scene/octree.h"

namespace hyperion::v2 {

Octree::~Octree()
{
    AssertThrowMsg(m_nodes.empty(), "Expected nodes to be emptied before octree destructor");
}

void Octree::SetParent(Octree *parent)
{
    m_parent = parent;

    if (m_parent != nullptr) {
        m_root = m_parent->m_root;
    } else {
        m_root = nullptr;
    }
}

void Octree::InitOctants()
{
    const Vector3 divided_aabb_dimensions = m_aabb.GetDimensions() / 2;

    for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
            for (int z = 0; z < 2; z++) {
                int index = 4 * x + 2 * y + z;

                m_octants[index] = {
                    .aabb = BoundingBox(
                        m_aabb.GetMin() + (divided_aabb_dimensions * Vector3(float(x), float(y), float(z))),
                        m_aabb.GetMin() + (divided_aabb_dimensions * (Vector3(float(x), float(y), float(z)) + Vector3(1.0f)))
                    )
                };
            }
        }
    }
}

void Octree::Divide(Engine *engine)
{
    AssertExit(!m_is_divided);

    for (auto &octant : m_octants) {
        AssertExit(octant.octree == nullptr);

        octant.octree = std::make_unique<Octree>(octant.aabb);
        octant.octree->SetParent(this);

        if (m_root != nullptr) {
            m_root->events.on_insert_octant(engine, octant.octree.get(), nullptr);
        }
    }

    m_is_divided = true;
}

void Octree::Undivide(Engine *engine)
{
    AssertExit(m_is_divided);

    for (auto &octant : m_octants) {
        AssertExit(octant.octree != nullptr);

        octant.octree->Clear(engine);
        
        if (m_root != nullptr) {
            m_root->events.on_remove_octant(engine, octant.octree.get(), nullptr);
        }

        octant.octree.reset();
    }

    m_is_divided = false;
}

void Octree::Clear(Engine *engine)
{
    if (m_root != nullptr) {
        for (const auto &node : m_nodes) {
            auto it = m_root->node_to_octree.find(node.spatial);

            if (it != m_root->node_to_octree.end()) {
                m_root->node_to_octree.erase(it);
            }
        }
    }

    m_nodes.clear();

    if (!m_is_divided) {
        return;
    }

    for (auto &octant : m_octants) {
        AssertExit(octant.octree != nullptr);

        octant.octree->Clear(engine);
        
        if (m_root != nullptr) {
            m_root->events.on_remove_octant(engine, octant.octree.get(), nullptr);
        }

        octant.octree.reset();
    }

    m_is_divided = false;
}

bool Octree::Insert(Engine *engine, Spatial *spatial)
{
    for (Octant &octant : m_octants) {
        if (octant.aabb.Contains(spatial->GetAabb())) {
            if (!m_is_divided) {
                Divide(engine);
            }

            AssertThrow(octant.octree != nullptr);

            return octant.octree->Insert(engine, spatial);
        }
    }

    return InsertInternal(engine, spatial);
}

bool Octree::InsertInternal(Engine *engine, Spatial *spatial)
{
    m_nodes.push_back(Node{
        .spatial = spatial,
        .aabb = spatial->GetAabb()
    });

    /* TODO: Inc ref count */

    if (m_root != nullptr) {
        AssertThrowMsg(m_root->node_to_octree[spatial] == nullptr, "Spatial must not already be in octree hierarchy.");

        m_root->node_to_octree[spatial] = this;
        m_root->events.on_insert_node(engine, this, spatial);
    }

    return true;
}

bool Octree::Remove(Engine *engine, Spatial *spatial)
{
    if (m_root != nullptr) {
        const auto it = m_root->node_to_octree.find(spatial);

        if (it == m_root->node_to_octree.end()) {
            return false;
        }

        if (auto *octree = it->second) {
            m_root->node_to_octree.erase(it);

            return octree->RemoveInternal(engine, spatial);
        }

        return false;
    }

    /* TODO: Dec ref count */

    if (!m_aabb.Contains(spatial->GetAabb())) {
        return false;
    }

    return RemoveInternal(engine, spatial);
}

bool Octree::RemoveInternal(Engine *engine, Spatial *spatial)
{
    const auto it = FindNode(spatial);

    if (it == m_nodes.end()) {
        if (m_is_divided) {
            for (auto &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->RemoveInternal(engine, spatial)) {
                    return true;
                }
            }
        } else {
            return false;
        }
    }

    m_nodes.erase(it);

    if (m_root != nullptr) {
        m_root->events.on_remove_node(engine, this, spatial);
    }

    return true;
}

bool Octree::Update(Engine *engine, Spatial *spatial)
{
    if (m_root != nullptr) {
        const auto it = m_root->node_to_octree.find(spatial);

        if (it == m_root->node_to_octree.end()) {
            return false;
        }

        if (auto *octree = it->second) {
            return octree->UpdateInternal(engine, spatial);
        }

        return false;
    }

    return UpdateInternal(engine, spatial);
}

bool Octree::UpdateInternal(Engine *engine, Spatial *spatial)
{
    const auto it = FindNode(spatial);

    if (it == m_nodes.end()) {
        if (m_is_divided) {
            for (auto &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->UpdateInternal(engine, spatial)) {
                    return true;
                }
            }
        } else {
            return false;
        }
    }

    const auto &new_aabb = spatial->GetAabb();
    const auto &old_aabb = it->aabb;

    if (new_aabb == old_aabb) {
        /* Aabb has not changed - no need to update */
        return true;
    }

    /*Aabb has changed to we remove it from this octree and either:
     * If we don't contain it anymore - insert it from the highest level octree that still contains the aabb and then walking down from there
     * If we do still contain it - we will remove it from this octree and re-insert it to find the deepest child octant
     */

    if (m_root != nullptr) {
        m_root->events.on_remove_node(engine, this, spatial);
        m_root->node_to_octree[spatial] = nullptr;
    }

    m_nodes.erase(it);

    if (IsRoot() || m_aabb.Contains(new_aabb)) {
        return Insert(engine, spatial);
    }

    Octree *parent = m_parent;

    /* Contains is false at this point */

    while (parent != nullptr) {
        if (parent->m_aabb.Contains(new_aabb)) {
            break;
        }

        parent = parent->m_parent;
    } ;

    if (parent != nullptr) {
        AssertThrow(parent->InsertInternal(engine, spatial));
    }

    /* Node has now been added -- remove any potential empty octants */
    if (!m_is_divided && m_nodes.empty()) {
        if (Octree *parent = m_parent) {
            while (parent->m_nodes.empty()) {
                AssertThrowMsg(parent->m_is_divided, "Should not have undivided octants throughout the octree");

                if (parent->m_parent == nullptr) {
                    /* At top level -- will be no more iterations after this, so undivide here. */
                    parent->Undivide(engine);

                    break;
                }
            }
        }
    }

    return true;
}


} // namespace hyperion::v2