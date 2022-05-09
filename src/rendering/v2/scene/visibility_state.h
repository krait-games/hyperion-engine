#ifndef HYPERION_V2_VISIBILITY_STATE_H
#define HYPERION_V2_VISIBILITY_STATE_H

#include "scene.h"

#include <util.h>

#include <atomic>
#include <cstdint>

namespace hyperion::v2 {

struct VisibilityState {
    using Bitmask = uint64_t;
    using Nonce   = uint16_t;

    static constexpr uint32_t max_scenes = sizeof(Bitmask) * CHAR_BIT;
    static constexpr uint32_t cursor_size = 2;

    /* map from scene index (id - 1) -> visibility boolean for each frame in flight.
     * when visiblity is scanned, both values per frame in flight are set accordingly,
     * but are only set to false after the corresponding frame is rendered.
     */
    std::atomic<Bitmask> bits;

    /* nonce is used to validate that the VisibilityState is still valid relative to
     * some parent VisibilityState. While the parent's nonce may have been set to a new
     * value, if ours has not, the state is considered to be invalid and thus not be
     * used as if it is in a "visible" state.
     */
    std::array<std::atomic<Nonce>, cursor_size> nonces;

    HYP_FORCE_INLINE bool Get(Scene::ID scene) const
    {
        AssertThrow(scene.value - 1ull < max_scenes);
        
        return bits & 1ull << static_cast<Bitmask>(scene.value - 1);
    }

    HYP_FORCE_INLINE void Set(Scene::ID scene, bool visible)
    {
        AssertThrow(scene.value - 1 < max_scenes);
        
        if (visible) {
            bits |= 1ull << static_cast<Bitmask>(scene.value - 1);
        } else {
            bits &= ~(1ull << (scene.value - 1));
        }
    }

    HYP_FORCE_INLINE bool ValidToParent(const VisibilityState &parent, uint32_t cursor) const
        { return nonces[cursor] == parent.nonces[cursor]; }
};

} // namespace hyperion::v2

#endif