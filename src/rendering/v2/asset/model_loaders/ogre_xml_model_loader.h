#ifndef HYPERION_V2_OGRE_XML_MODEL_LOADER_H
#define HYPERION_V2_OGRE_XML_MODEL_LOADER_H

#include <rendering/v2/asset/loader_object.h>
#include <rendering/v2/asset/loader.h>
#include <rendering/v2/components/mesh.h>
#include <rendering/v2/scene/node.h>

namespace hyperion::v2 {

template <>
struct LoaderObject<Node, LoaderFormat::OGRE_XML_MODEL> {
    class Loader : public LoaderBase<Node, LoaderFormat::OGRE_XML_MODEL> {
        static LoaderResult LoadFn(LoaderState *state, Object &);
        static std::unique_ptr<Node> BuildFn(Engine *engine, const Object &);

    public:
        Loader()
            : LoaderBase({
                .load_fn = LoadFn,
                .build_fn = BuildFn
            })
        {
        }
    };

    struct OgreSubMesh {
        std::vector<Mesh::Index> indices;
    };

    struct BoneAssignment {
        size_t index;
        float  weight;
    };

    std::string filepath;

    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;

    std::vector<Vertex> vertices;

    std::vector<OgreSubMesh> submeshes;
    std::unordered_map<size_t, std::vector<BoneAssignment>> bone_assignments;

    std::string skeleton_name;
};

} // namespace hyperion::v2

#endif