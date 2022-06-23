#include "octree.h"
#include "spatial.h"
#include "../engine.h"
#include <threads.h>

namespace hyperion::v2 {

const BoundingBox Octree::default_bounds = BoundingBox({-250.0f}, {250.0f});

bool Octree::IsVisible(const Octree *root, const Octree *child)
{
    return child->m_visibility_state.ValidToParent(
        root->m_visibility_state,
        (root->GetRoot()->visibility_cursor + VisibilityState::cursor_size - 1) % VisibilityState::cursor_size
    );
}

Octree::Octree(const BoundingBox &aabb)
    : Octree(nullptr, aabb, 0)
{
    m_root = new Root;
}

Octree::Octree(Octree *parent, const BoundingBox &aabb, UInt8 index)
    : m_parent(nullptr),
      m_aabb(aabb),
      m_is_divided(false),
      m_root(nullptr),
      m_visibility_state{},
      m_index(index)
{
    if (parent != nullptr) {
        SetParent(parent); // call explicitly to set root ptr
    }

    InitOctants();
}

Octree::~Octree()
{
    if (IsRoot()) {
        delete m_root;
    }

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

bool Octree::EmptyDeep(int depth, UInt8 octant_mask) const
{
    if (!Empty()) {
        return false;
    }

    if (!m_is_divided) {
        return true;
    }

    if (depth != 0) {
        return std::all_of(m_octants.begin(), m_octants.end(), [depth, octant_mask](const Octant &octant) {
            if (octant_mask & (1u << octant.octree->m_index)) {
                return octant.octree->EmptyDeep(depth - 1);
            }

            return true;
        });
    }

    return true;
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
                        m_aabb.GetMin() + divided_aabb_dimensions * Vector3(float(x), float(y), float(z)),
                        m_aabb.GetMin() + divided_aabb_dimensions * (Vector3(float(x), float(y), float(z)) + Vector3(1.0f))
                    )
                };
            }
        }
    }
}

void Octree::Divide(Engine *engine)
{
    AssertThrow(!m_is_divided);

    for (size_t i = 0; i < m_octants.size(); i++) {
        auto &octant = m_octants[i];

        AssertThrow(octant.octree == nullptr);

        octant.octree.reset(new Octree(this, octant.aabb, static_cast<UInt8>(i)));

        if (m_root != nullptr) {
            m_root->events.on_insert_octant(engine, octant.octree.get(), nullptr);
        }
    }

    m_is_divided = true;
}

void Octree::Undivide(Engine *engine)
{
    AssertThrow(m_is_divided);
    AssertThrowMsg(m_nodes.empty(), "Undivide() should be called on octrees with no remaining nodes");

    for (auto &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->m_is_divided) {
            octant.octree->Undivide(engine);
        }
        
        if (m_root != nullptr) {
            m_root->events.on_remove_octant(engine, octant.octree.get(), nullptr);
        }

        octant.octree.reset();
    }

    m_is_divided = false;
}

void Octree::CollapseParents(Engine *engine)
{
    if (m_is_divided || !Empty()) {
        return;
    }
    DebugLog(LogType::Debug, "collapse parents\n");

    Octree *iteration = m_parent,
           *highest_empty = nullptr;

    while (iteration != nullptr && iteration->Empty()) {
        for (const Octant &octant : iteration->m_octants) {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree.get() == highest_empty) {
                /* Do not perform a check on our node, as we've already
                 * checked it by going up the chain.
                 * As `iter` becomes the parent of this node we're currently working with,
                 * we will not have to perform duplicate EmptyDeep() checks on any octants because of this check.
                 */
                continue;
            }

            if (!octant.octree->EmptyDeep()) {
                goto undivide;
            }
        }

        highest_empty = iteration;
        iteration = iteration->m_parent;
    }

undivide:
    if (highest_empty != nullptr) {
        highest_empty->Undivide(engine);
    }
}

void Octree::Clear(Engine *engine)
{
    std::vector<Node> nodes;

    Clear(engine, nodes);
}

void Octree::Clear(Engine *engine, std::vector<Node> &out_nodes)
{
    ClearInternal(engine, out_nodes);

    if (m_is_divided) {
        Undivide(engine);
    }
}

void Octree::ClearInternal(Engine *engine, std::vector<Node> &out_nodes)
{
    for (auto &node : m_nodes) {
        node.spatial->OnRemovedFromOctree(this);

        if (m_root != nullptr) {
            auto it = m_root->node_to_octree.find(node.spatial);

            if (it != m_root->node_to_octree.end()) {
                m_root->node_to_octree.erase(it);
            }
        }

        out_nodes.push_back(std::move(node));
    }

    m_nodes.clear();

    if (!m_is_divided) {
        return;
    }

    for (auto &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        octant.octree->ClearInternal(engine, out_nodes);
    }
}

Octree::Result Octree::Insert(Engine *engine, Spatial *spatial)
{
    AssertThrow(spatial != nullptr);

    const auto &spatial_aabb = spatial->GetWorldAabb();

    if (!m_aabb.Contains(spatial_aabb)) {
        auto rebuild_result = RebuildExtendInternal(engine, spatial_aabb);

        if (!rebuild_result) {
            DebugLog(
                LogType::Warn,
                "Failed to rebuild octree when inserting spatial #%lu\n",
                spatial->GetId().value
            );

            return rebuild_result;
        }
    }

    for (Octant &octant : m_octants) {
        if (octant.aabb.Contains(spatial_aabb)) {
            if (!m_is_divided) {
                Divide(engine);
            }

            AssertThrow(octant.octree != nullptr);

            return octant.octree->Insert(engine, spatial);
        }
    }

    return InsertInternal(engine, spatial);
}

Octree::Result Octree::InsertInternal(Engine *engine, Spatial *spatial)
{
    m_nodes.push_back(Node {
        .spatial          = spatial,
        .aabb             = spatial->GetWorldAabb(),
        .visibility_state = &m_visibility_state
    });

    spatial->OnAddedToOctree(this);

    if (m_root != nullptr) {
        AssertThrowMsg(m_root->node_to_octree.find(spatial) == m_root->node_to_octree.end(), "Spatial must not already be in octree hierarchy.");

        m_root->node_to_octree[spatial] = this;
        m_root->events.on_insert_node(engine, this, spatial);
    }

    return {};
}

Octree::Result Octree::Remove(Engine *engine, Spatial *spatial)
{
    if (m_root != nullptr) {
        const auto it = m_root->node_to_octree.find(spatial);

        if (it == m_root->node_to_octree.end()) {
            return {Result::OCTREE_ERR, "Not found in node map"};
        }

        if (auto *octree = it->second) {
            m_root->node_to_octree.erase(it);

            return octree->RemoveInternal(engine, spatial);
        }

        return {Result::OCTREE_ERR, "Could not be removed from any sub octants"};
    }

    if (!m_aabb.Contains(spatial->GetWorldAabb())) {
        return {Result::OCTREE_ERR, "AABB does not contain object aabb"};
    }

    return RemoveInternal(engine, spatial);
}

Octree::Result Octree::RemoveInternal(Engine *engine, Spatial *spatial)
{
    const auto it = FindNode(spatial);

    if (it == m_nodes.end()) {
        if (m_is_divided) {
            for (auto &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->RemoveInternal(engine, spatial)) {
                    return {};
                }
            }
        }

        return {Result::OCTREE_ERR, "Could not be removed from any sub octants"};
    }

    if (m_root != nullptr) {
        m_root->events.on_remove_node(engine, this, spatial);
        m_root->node_to_octree.erase(spatial);
    }

    spatial->OnRemovedFromOctree(this);

    m_nodes.erase(it);

    if (!m_is_divided && m_nodes.empty()) {
        Octree *last_empty_parent = nullptr;

        if (Octree *parent = m_parent) {
            const Octree *child = this;

            while (parent->EmptyDeep(DEPTH_SEARCH_INF, 0xff & ~(1 << child->m_index))) { // do not search this branch of the tree again
                last_empty_parent = parent;

                if (parent->m_parent == nullptr) {
                    break;
                }

                child  = parent;
                parent = child->m_parent;
            }
        }

        if (last_empty_parent != nullptr) {
            AssertThrow(last_empty_parent->EmptyDeep(DEPTH_SEARCH_INF));

            /* At highest empty parent octant, call Undivide() to collapse nodes */
            last_empty_parent->Undivide(engine);
        }
    }

    return {};
}

Octree::Result Octree::Move(Engine *engine, Spatial *spatial, const std::vector<Node>::iterator *it)
{
    const auto &new_aabb = spatial->GetWorldAabb();

    const bool is_root = IsRoot();
    const bool contains = m_aabb.Contains(new_aabb);

    if (!contains) {
        // NO LONGER CONTAINS AABB

        if (is_root) {
            // have to rebuild, invalidating child octants.
            // which we have a Contains() check for child nodes walking upwards

#if HYP_OCTREE_DEBUG
            DebugLog(
                LogType::Debug,
                "In root, but does not contain node aabb, so rebuilding octree. %lu\n",
                spatial->GetId().value
            );
#endif

            return RebuildExtendInternal(engine, new_aabb);
        }

        // not root
#if HYP_OCTREE_DEBUG
            DebugLog(
                LogType::Debug,
                "Moving spatial #%lu into the closest fitting (or root) parent\n",
                spatial->GetId().value
            );
#endif


        bool inserted = false;

        /* Contains is false at this point */
        Octree *parent      = m_parent;
        Octree *last_parent = m_parent;

        while (parent != nullptr) {
            last_parent = parent;

            if (parent->m_aabb.Contains(new_aabb)) {
                
                if (it != nullptr) {
                    if (m_root != nullptr) {
                        m_root->events.on_remove_node(engine, this, spatial);
                        m_root->node_to_octree.erase(spatial);
                    }

                    m_nodes.erase(*it);
                }


                inserted = bool(parent->Move(engine, spatial));

                break;
            }

            parent = parent->m_parent;
        }

        if (inserted) { // succesfully inserted, safe to call CollapseParents()
            /* Node has now been added to it's appropriate octant -- remove any potential empty octants */
            CollapseParents(engine);

            return {};
        }

        // not inserted because no Move() was called on parents (because they don't contain AABB),
        // have to _manually_ call Move() which will go to the above branch for the parent octant,
        // this invalidating `this`

#if HYP_OCTREE_DEBUG
                DebugLog(
                    LogType::Debug,
                    "In child, no parents contain AABB so calling Move() on last valid octant (root). This will invalidate `this`.. %lu\n",
                    spatial->GetId().value
                );
#endif

        AssertThrow(last_parent != nullptr);

        return last_parent->Move(engine, spatial);
    }

    // CONTAINS AABB HERE
    
    for (Octant &octant : m_octants) {
        if (octant.aabb.Contains(new_aabb)) {
            /* we /can/ go a level deeper than current, so no matter what we dispatch the
             * event that the spatial was 'removed' from this octant, as it will be added
             * deeper regardless.
             * note: we don't call OnRemovedFromOctree() on the spatial, we don't want the Spatial's
             * m_octree ptr to be nullptr at any point, which could cause flickering when we go to render
             * stuff. that's the purpose for this whole method, to set it in one pass
             */
            if (it != nullptr) {
                if (m_root != nullptr) {
                    m_root->events.on_remove_node(engine, this, spatial);
                    m_root->node_to_octree.erase(spatial);
                }

                m_nodes.erase(*it);
            }

            if (!m_is_divided) {
                Divide(engine);
            }
            
            AssertThrow(octant.octree != nullptr);

            const auto octant_move_result = octant.octree->Move(engine, spatial, nullptr);
            AssertThrow(octant_move_result);

            return octant_move_result;
        }
    }

    if (it != nullptr) { /* Not moved */
        auto &spatial_it = *it;

        spatial_it->aabb             = new_aabb;
        spatial_it->visibility_state = &m_visibility_state;
    } else { /* Moved into new octant */
        

        /* HACK to prevent objects having no visibility state and flicking when switching octants.
         * We set the visibility state to be the most up to date it can be, and in the next visibility
         * state check, the proper state will be applied
         */
        if (auto *current_octant = spatial->GetOctree()) {
            if (spatial->GetScene() == nullptr) {
                DebugLog(
                    LogType::Error,
                    "Spatial #%u was not attached to a Scene!\n",
                    spatial->GetId().value
                );

                return {Result::OCTREE_ERR, "Object not attached to a scene!"};
            }

            if (m_root != nullptr) {
                const UInt32 curr = m_root->visibility_cursor.load();

                m_visibility_state.nonces[curr] = spatial->GetScene()->GetOctree().GetVisibilityState().nonces[curr].load();
                //m_visibility_state.bits         |= current_octant->GetVisibilityState().bits.load();
            }
        }

        m_nodes.push_back(Node {
            .spatial          = spatial,
            .aabb             = spatial->GetWorldAabb(),
            .visibility_state = &m_visibility_state
        });

        spatial->OnMovedToOctant(this);

        if (m_root != nullptr) {
            AssertThrowMsg(m_root->node_to_octree.find(spatial) == m_root->node_to_octree.end(), "Spatial must not already be in octree hierarchy.");

            m_root->node_to_octree[spatial] = this;
            m_root->events.on_insert_node(engine, this, spatial);
        }

    }

    
    return {};
}

Octree::Result Octree::Update(Engine *engine, Spatial *spatial)
{
    if (m_root != nullptr) {
        const auto it = m_root->node_to_octree.find(spatial);

        if (it == m_root->node_to_octree.end()) {
            return {Result::OCTREE_ERR, "Object not found in node map!"};
        }

        if (auto *octree = it->second) {
            return octree->UpdateInternal(engine, spatial);
        }

        return {Result::OCTREE_ERR, "Object has no octree in node map!"};
    }

    return UpdateInternal(engine, spatial);
}

Octree::Result Octree::UpdateInternal(Engine *engine, Spatial *spatial)
{
    const auto it = FindNode(spatial);

    if (it == m_nodes.end()) {
        if (m_is_divided) {
            for (auto &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->UpdateInternal(engine, spatial)) {
                    return {};
                }
            }
        }

        return {Result::OCTREE_ERR, "Could not update in any sub octants"};
    }

    const auto &new_aabb = spatial->GetWorldAabb();
    const auto &old_aabb = it->aabb;

    if (new_aabb == old_aabb) {
        /* Aabb has not changed - no need to update */
        return {};
    }

    /* Aabb has changed to we remove it from this octree and either:
     * If we don't contain it anymore - insert it from the highest level octree that still contains the aabb and then walking down from there
     * If we do still contain it - we will remove it from this octree and re-insert it to find the deepest child octant
     */

    return Move(engine, spatial, &it);
}

Octree::Result Octree::Rebuild(Engine *engine, const BoundingBox &new_aabb)
{
    std::vector<Node> new_nodes;

    Clear(engine, new_nodes);

    m_aabb = new_aabb;

    for (auto &node : new_nodes) {
        auto insert_result = Insert(engine, node.spatial);

        if (!insert_result) {
            return insert_result;
        }
    }

    return {};
}

Octree::Result Octree::RebuildExtendInternal(Engine *engine, const BoundingBox &extend_include_aabb)
{
    // have to grow the aabb by rebuilding the octree
    BoundingBox new_aabb(m_aabb);
    // extend the new aabb to include the spatial
    new_aabb.Extend(extend_include_aabb);
    // grow our new aabb by a predetermined growth factor,
    // to keep it from constantly resizing
    new_aabb *= growth_factor;

    return Rebuild(engine, new_aabb);
}

void Octree::CalculateVisibility(Scene *scene)
{
    if (scene == nullptr) {
        return;
    }

    if (scene->GetId().value - 1 >= VisibilityState::max_scenes) {
        DebugLog(
            LogType::Error,
            "Scene #%lu out of bounds of octree scene visibility bitset. Cannot update visibility state.\n",
            scene->GetId().value
        );

        return;
    }

    const auto &frustum = scene->GetCamera()->GetFrustum();
     
    if (frustum.ContainsAabb(m_aabb)) {
        m_root->visibility_cursor = (m_root->visibility_cursor + 1) % VisibilityState::cursor_size;

        ++m_visibility_state.nonces[m_root->visibility_cursor];

        UpdateVisibilityState(scene);
    }
}

void Octree::UpdateVisibilityState(Scene *scene)
{
    AssertThrow(m_root != nullptr);

    /* assume we are already visible from CalculateVisibility() check */
    const auto &frustum = scene->GetCamera()->GetFrustum();

    m_visibility_state.Set(scene->GetId(), true);

    if (!m_is_divided) {
        return;
    }

    const auto cursor = m_root->visibility_cursor.load();

    const auto nonce = m_visibility_state.nonces[cursor].load();

    for (auto &octant : m_octants) {
        if (!frustum.ContainsAabb(octant.aabb)) {
            continue;
        }

        octant.octree->m_visibility_state.nonces[cursor] = nonce;
        octant.octree->UpdateVisibilityState(scene);
    }
}

void Octree::OnSpatialRemoved(Engine *engine, Spatial *spatial)
{
    if (spatial == nullptr) {
        return;
    }
    
    if (!Remove(engine, spatial)) {
        DebugLog(LogType::Error, "Failed to find Spatial #%lu in octree\n", spatial->GetId().value);
    }
}

bool Octree::TestRay(const Ray &ray, RayTestResults &out_results) const
{
    bool has_hit = false;

    if (ray.TestAabb(m_aabb)) {
        for (auto &node : m_nodes) {
            if (ray.TestAabb(
                node.aabb,
                node.spatial->GetId().value,
                static_cast<void *>(node.spatial),
                out_results
            )) {
                has_hit = true;
            }
        }
        
        if (m_is_divided) {
            for (auto &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->TestRay(ray, out_results)) {
                    has_hit = true;
                }
            }
        }
    }

    return has_hit;
}

} // namespace hyperion::v2