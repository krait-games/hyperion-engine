#include "svo.h"
#include"../engine.h"

#include <asset/asset_manager.h>

namespace hyperion::v2 {

using renderer::StagingBuffer;
using renderer::Result;
using ResourceState = renderer::GPUMemory::ResourceState;
using Context = renderer::StagingBufferPool::Context;

inline static constexpr uint32_t group_x_64(uint32_t x) { return (x >> 6u) + ((x & 0x3fu) ? 1u : 0u); }

SparseVoxelOctree::SparseVoxelOctree()
    : EngineComponentBase()
{
}

SparseVoxelOctree::~SparseVoxelOctree()
{
    Teardown();
}

void SparseVoxelOctree::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_VOXELIZER, [this](Engine *engine) {
        if (m_voxelizer == nullptr) {
            m_voxelizer = std::make_unique<Voxelizer>();
            m_voxelizer->Init(engine);
        }

        if (m_counter == nullptr) {
            m_counter = std::make_unique<AtomicCounter>();
            m_counter->Create(engine);
        }

        /* For now, until revoxelization is implemented */
        AssertThrow(m_octree_buffer == nullptr);

        CreateBuffers(engine);
        CreateDescriptors(engine);
        CreateComputePipelines(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_VOXELIZER, [this](Engine *engine) {
            auto result = Result::OK;

            m_voxelizer.reset(nullptr);

            if (m_counter != nullptr) {
                m_counter->Destroy(engine);
                m_counter.reset(nullptr);
            }

            if (m_build_info_buffer != nullptr) {
                HYPERION_PASS_ERRORS(
                    m_build_info_buffer->Destroy(engine->GetInstance()->GetDevice()),
                    result
                );
            }

            if (m_indirect_buffer != nullptr) {
                HYPERION_PASS_ERRORS(
                    m_indirect_buffer->Destroy(engine->GetInstance()->GetDevice()),
                    result
                );
            }

            if (m_octree_buffer != nullptr) {
                HYPERION_PASS_ERRORS(
                    m_octree_buffer->Destroy(engine->GetInstance()->GetDevice()),
                    result
                );
            }

            m_build_info_buffer.reset(nullptr);
            m_indirect_buffer.reset(nullptr);
            m_octree_buffer.reset(nullptr);
        
            m_alloc_nodes = nullptr;
            m_init_nodes = nullptr;
            m_tag_nodes = nullptr;
            m_modify_args = nullptr;
            m_write_mipmaps = nullptr;

            HYPERION_ASSERT_RESULT(result);
        }), engine);
    }));
}

size_t SparseVoxelOctree::CalculateNumNodes() const
{
    const size_t fragment_count = m_voxelizer->NumFragments();

    size_t num_nodes = MathUtil::Max(fragment_count << 3u, min_nodes);

    if (num_nodes > max_nodes) {
        DebugLog(
            LogType::Warn,
            "Attempt to allocate %llu nodes which is greater than maximum of %llu, capping at max\n",
            num_nodes,
            max_nodes
        );

        num_nodes = max_nodes;
    }

    return num_nodes;
}

void SparseVoxelOctree::CreateBuffers(Engine *engine)
{
    auto result = Result::OK;

    m_build_info_buffer = std::make_unique<StorageBuffer>();

    HYPERION_PASS_ERRORS(
        m_build_info_buffer->Create(engine->GetInstance()->GetDevice(), 2 * sizeof(uint32_t)),
        result
    );

    m_indirect_buffer = std::make_unique<IndirectBuffer>();

    HYPERION_PASS_ERRORS(
        m_indirect_buffer->Create(engine->GetInstance()->GetDevice(), 3 * sizeof(uint32_t)),
        result
    );

    /* TODO resizing / modifying descriptors for revoxelization: should a Ref<> to a descriptor wrapper... ? */

    m_octree_buffer = std::make_unique<StorageBuffer>();

    const size_t num_nodes = CalculateNumNodes();

    DebugLog(
        LogType::Debug,
        "%llu rendered fragments, creating %llu octree nodes (%llu MiB)\n",
        m_voxelizer->NumFragments(),
        num_nodes,
        (num_nodes * sizeof(OctreeNode)) / 1000000
    );

    HYPERION_PASS_ERRORS(
        m_octree_buffer->Create(engine->GetInstance()->GetDevice(), num_nodes * sizeof(OctreeNode)),
        result
    );

    if (!result) {
        HYPERION_PASS_ERRORS(
            m_octree_buffer->Destroy(engine->GetInstance()->GetDevice()),
            result
        );

        m_octree_buffer.reset(nullptr);

        HYPERION_PASS_ERRORS(
            m_build_info_buffer->Destroy(engine->GetInstance()->GetDevice()),
            result
        );

        m_build_info_buffer.reset(nullptr);

        HYPERION_PASS_ERRORS(
            m_indirect_buffer->Destroy(engine->GetInstance()->GetDevice()),
            result
        );

        m_indirect_buffer.reset(nullptr);

        HYPERION_ASSERT_RESULT(result);
    }
}

void SparseVoxelOctree::CreateDescriptors(Engine *engine)
{
    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

    /* 0 = voxel atomic counter */
    /* 1 = fragment list  */
    /* 2 = octree         */
    /* 3 = build info     */
    /* 4 = indirect       */
    /* 5 = octree atomic counter */
    descriptor_set->AddDescriptor<renderer::StorageBufferDescriptor>(2)
        ->AddSubDescriptor({.buffer = m_octree_buffer.get()});

    descriptor_set->AddDescriptor<renderer::StorageBufferDescriptor>(3)
        ->AddSubDescriptor({.buffer = m_build_info_buffer.get()});

    descriptor_set->AddDescriptor<renderer::StorageBufferDescriptor>(4)
        ->AddSubDescriptor({.buffer = m_indirect_buffer.get()});

    descriptor_set
        ->AddDescriptor<renderer::StorageBufferDescriptor>(5)
        ->AddSubDescriptor({.buffer = m_counter->GetBuffer()});
}

void SparseVoxelOctree::CreateComputePipelines(Engine *engine)
{
    m_alloc_nodes = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/voxel/octree_alloc_nodes.comp.spv").Read()}}
            }
        ))
    ));

    m_init_nodes = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/voxel/octree_init_nodes.comp.spv").Read()}}
            }
        ))
    ));

    m_tag_nodes = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/voxel/octree_tag_nodes.comp.spv").Read()}}
            }
        ))
    ));

    m_modify_args = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/voxel/octree_modify_args.comp.spv").Read()}}
            }
        ))
    ));

    m_write_mipmaps = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/voxel/octree_write_mipmaps.comp.spv").Read()}}
            }
        ))
    ));
}

void SparseVoxelOctree::Build(Engine *engine)
{
    AssertThrow(m_voxelizer != nullptr);
    m_voxelizer->Render(engine);

    m_counter->Reset(engine);

    static constexpr uint32_t build_info[] = {0, 8};
    static constexpr uint32_t indirect_info[] = {1, 1, 1};

    const renderer::ComputePipeline::PushConstantData push_constants{
        .octree_data = {
            .num_fragments   = m_voxelizer->NumFragments(),
            .voxel_grid_size = static_cast<uint32_t>(m_voxelizer->voxel_map_size)
        }
    };

    /* resize node buffer */
    const size_t num_nodes = CalculateNumNodes();

    if (num_nodes * sizeof(OctreeNode) > m_octree_buffer->size) {
        DebugLog(
            LogType::Debug,
            "Resizing octree buffer to %llu nodes (%llu MiB)\n",
            num_nodes,
            (num_nodes * sizeof(OctreeNode)) / 1000000
        );

        HYPERION_ASSERT_RESULT(m_octree_buffer->Destroy(engine->GetDevice()));

        auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);
        
        m_octree_buffer.reset(new StorageBuffer());
        HYPERION_ASSERT_RESULT(m_octree_buffer->Create(engine->GetDevice(), num_nodes * sizeof(OctreeNode)));

        descriptor_set->GetDescriptor(2)->RemoveSubDescriptor(0);
        descriptor_set->GetDescriptor(2)->AddSubDescriptor({.buffer = m_octree_buffer.get()});
        descriptor_set->ApplyUpdates(engine->GetInstance()->GetDevice());
    }
    
    HYPERION_ASSERT_RESULT(engine->GetInstance()->GetStagingBufferPool().Use(
        engine->GetInstance()->GetDevice(),
        [&](Context &context) {
            auto *device = engine->GetDevice();

            StagingBuffer *build_info_staging_buffer = context.Acquire(m_build_info_buffer->size);
            build_info_staging_buffer->Copy(device, std::size(build_info) * sizeof(uint32_t), build_info);

            StagingBuffer *indirect_staging_buffer = context.Acquire(m_indirect_buffer->size);
            indirect_staging_buffer->Copy(device, std::size(indirect_info) * sizeof(uint32_t), indirect_info);

            auto commands = engine->GetInstance()->GetSingleTimeCommands();

            /* Copy our data from staging buffers */
            commands.Push([&](CommandBuffer *command_buffer) {
                m_build_info_buffer->CopyFrom(command_buffer, build_info_staging_buffer, std::size(build_info) * sizeof(uint32_t));
                m_build_info_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);

                m_indirect_buffer->CopyFrom(command_buffer, indirect_staging_buffer, std::size(indirect_info) * sizeof(uint32_t));
                m_indirect_buffer->InsertBarrier(command_buffer, ResourceState::INDIRECT_ARG);

                HYPERION_RETURN_OK;
            });

	        uint32_t fragment_group_x = group_x_64(m_voxelizer->NumFragments());

            for (size_t i = 1; i <= m_voxelizer->octree_depth; i++) {
                commands.Push([&, index = i](CommandBuffer *command_buffer) {
                    m_init_nodes->GetPipeline()->Bind(command_buffer, push_constants);
                    BindDescriptorSets(engine, command_buffer, m_init_nodes.ptr);
                    m_init_nodes->GetPipeline()->DispatchIndirect(command_buffer, m_indirect_buffer.get());

                    m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);

                    m_tag_nodes->GetPipeline()->Bind(command_buffer, push_constants);
                    BindDescriptorSets(engine, command_buffer, m_tag_nodes.ptr);
                    m_tag_nodes->GetPipeline()->Dispatch(command_buffer, {fragment_group_x, 1, 1});

                    if (index == m_voxelizer->octree_depth) {
                        HYPERION_RETURN_OK;
                    }
                    
                    m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);
                    
                    m_alloc_nodes->GetPipeline()->Bind(command_buffer, push_constants);
                    BindDescriptorSets(engine, command_buffer, m_alloc_nodes.ptr);
                    m_alloc_nodes->GetPipeline()->DispatchIndirect(command_buffer, m_indirect_buffer.get());

                    m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);

                    m_modify_args->GetPipeline()->Bind(command_buffer);
                    BindDescriptorSets(engine, command_buffer, m_modify_args.ptr);
                    m_modify_args->GetPipeline()->Dispatch(command_buffer, {1, 1, 1});
                    
                    m_indirect_buffer->InsertBarrier(command_buffer, ResourceState::INDIRECT_ARG);
                    m_build_info_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);

                    HYPERION_RETURN_OK;
                });
            }

            return commands.Execute(device);
        }
    ));

    WriteMipmaps(engine);
}

void SparseVoxelOctree::WriteMipmaps(Engine *engine)
{
    renderer::ComputePipeline::PushConstantData push_constants{
        .octree_data = {
            .num_fragments   = m_voxelizer->NumFragments(),
            .voxel_grid_size = static_cast<uint32_t>(m_voxelizer->voxel_map_size)
        }
    };

	const uint32_t fragment_group_x = group_x_64(m_voxelizer->NumFragments());

    auto commands = engine->GetInstance()->GetSingleTimeCommands();

    commands.Push([&](CommandBuffer *command_buffer) {
        for (uint32_t i = 2; i <= m_voxelizer->octree_depth; i++) {
            push_constants.octree_data.mipmap_level = i;

            m_write_mipmaps->GetPipeline()->Bind(command_buffer, push_constants);
            BindDescriptorSets(engine, command_buffer, m_write_mipmaps.ptr);
            m_write_mipmaps->GetPipeline()->Dispatch(command_buffer, {fragment_group_x, 1, 1});

            if (i != m_voxelizer->octree_depth) {
                m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);
            }
        }

        HYPERION_RETURN_OK;
    });

    HYPERION_ASSERT_RESULT(commands.Execute(engine->GetDevice()));
}

void SparseVoxelOctree::BindDescriptorSets(Engine *engine,
                                           CommandBuffer *command_buffer,
                                           ComputePipeline *pipeline) const
{
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetInstance()->GetDevice(),
        command_buffer,
        pipeline->GetPipeline(),
        {{
            .set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER,
            .count = 1
        }}
    );
}

} // namespace hyperion::v2