#include "fbom.h"

namespace hyperion {
namespace fbom {
FBOMType FBOMType::Extend(const FBOMType &object) const
{
    return FBOMObjectType(object.name, *this);
}
} // namespace fbom
} // namespace hyperion
