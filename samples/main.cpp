
#include "system/sdl_system.h"
#include "system/debug.h"
#include "rendering/backend/renderer_instance.h"
#include "rendering/backend/renderer_descriptor_set.h"
#include "rendering/backend/renderer_image.h"
#include "rendering/backend/renderer_render_pass.h"
#include "rendering/backend/rt/renderer_raytracing_pipeline.h"

#include <engine.h>
#include <scene/node.h>
#include <rendering/atomics.h>
#include <animation/bone.h>
#include <asset/model_loaders/obj_model_loader.h>
#include <rendering/rt/acceleration_structure_builder.h>
#include <rendering/probe_system.h>
#include <rendering/post_fx/ssao.h>
#include <rendering/post_fx/fxaa.h>
#include <rendering/post_fx/tonemap.h>
#include <scene/controllers/audio_controller.h>
#include <scene/controllers/animation_controller.h>
#include <scene/controllers/aabb_debug_controller.h>
#include <scene/controllers/follow_camera_controller.h>
#include <scene/controllers/paging/basic_paging_controller.h>
#include <scene/controllers/scripted_controller.h>
#include <core/lib/flat_set.h>
#include <core/lib/flat_map.h>
#include <game_thread.h>
#include <game.h>

#include <terrain/controllers/terrain_paging_controller.h>

#include <rendering/vct/vct.h>

#include <util/fs/fs_util.h>

#include <input/input_manager.h>
#include <camera/fps_camera.h>
#include <camera/follow_camera.h>

#include "util/profile.h"

/* Standard library */
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <unordered_map>
#include <string>
#include <cmath>
#include <thread>

#include "rendering/environment.h"
#include "rendering/render_components/cubemap_renderer.h"

#include <script/ScriptBindings.hpp>

#include <util/utf8.hpp>

using namespace hyperion;


#define HYPERION_VK_TEST_IMAGE_STORE 0
#define HYPERION_VK_TEST_ATOMICS     1
#define HYPERION_VK_TEST_VISUALIZE_OCTREE 0
#define HYPERION_VK_TEST_SPARSE_VOXEL_OCTREE 0
#define HYPERION_VK_TEST_VCT 1
#define HYPERION_VK_TEST_RAYTRACING 0
#define HYPERION_RUN_TESTS 1

v2::VoxelConeTracing *vct = nullptr; // Have to do this for now... we need some way of statically generating descriptor sets

namespace hyperion::v2 {
    
class MyGame : public v2::Game {

public:
    Ref<Material> base_material;//hack

    Ref<Light>    m_point_light;

    MyGame()
        : Game()
    {
    }

    virtual void Init(Engine *engine, SystemWindow *window) override
    {
        using namespace v2;

        Game::Init(engine, window);

        input_manager = new InputManager(window);
        input_manager->SetWindow(window);
        
        engine->GetDeferredRenderer().GetPostProcessing().AddEffect<SSAOEffect>();
        engine->GetDeferredRenderer().GetPostProcessing().AddEffect<FXAAEffect>();
        //engine->GetDeferredRenderer().GetPostProcessing().AddEffect<TonemapEffect>();

        // not sure why this needs to be in render thread atm?
        scene = engine->resources.scenes.Add(std::make_unique<v2::Scene>(
            std::make_unique<FpsCamera>(//FollowCamera>(
                //Vector3(0, 0, 0), Vector3(0, 0.5f, -2),
                1024, 768,
                70.0f,
                0.15f, 15000.0f
            )
        ));
    }

    virtual void OnPostInit(Engine *engine) override
    {
        engine->GetWorld().AddScene(scene.IncRef());

        base_material = engine->resources.materials.Add(std::make_unique<Material>());
        base_material.Init();

        auto loaded_assets = engine->assets.Load<Node>(
            "models/ogrexml/dragger_Body.mesh.xml",
            "models/sponza/sponza.obj", //
            "models/cube.obj",
            "models/material_sphere/material_sphere.obj",
            "models/grass/grass.obj"
        );

        zombie = std::move(loaded_assets[0]);
        test_model = std::move(loaded_assets[1]);
        cube_obj = std::move(loaded_assets[2]);
        material_test_obj = std::move(loaded_assets[3]);

        auto sphere = engine->assets.Load<Node>("models/sphere_hq.obj");
        sphere->Scale(2.0f);
        sphere->SetName("sphere");
        // sphere->GetChild(0)->GetSpatial()->SetMaterial(engine->resources.materials.Add(std::make_unique<Material>()));
        sphere->GetChild(0)->GetSpatial()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        sphere->GetChild(0)->GetSpatial()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.9f);
        sphere->GetChild(0)->GetSpatial()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 1.0f);
        //sphere->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/plastic/plasticpattern1-normal2-unity2b.png")));
        sphere->GetChild(0)->GetSpatial()->GetInitInfo().flags &= ~Spatial::ComponentInitInfo::Flags::ENTITY_FLAGS_RAY_TESTS_ENABLED;
        scene->GetRootNode()->AddChild(std::move(sphere));

        // auto character_entity = engine->resources.spatials.Add(std::make_unique<Spatial>());
        // character_entity->AddController<BasicCharacterController>();
        // scene->AddSpatial(std::move(character_entity));

        // auto tmp_terrain = engine->assets.Load<Node>("models/tmp_terrain.obj");
        // tmp_terrain->Rotate(Quaternion({ 1, 0, 0 }, MathUtil::DegToRad(90.0f)));
        // tmp_terrain->Scale(500.0f);
        // scene->AddSpatial(tmp_terrain->GetChild(0)->GetSpatial().IncRef());

        //auto terrain_node = scene->GetRootNode()->AddChild();
        //terrain_node->SetSpatial(engine->resources.spatials.Add(std::make_unique<Spatial>()));
        // terrain_node->GetSpatial()->AddController<TerrainPagingController>(888, Extent3D{128}, Vector3{12, 12, 12});
        
        
        auto *grass = scene->GetRootNode()->AddChild(std::move(loaded_assets[4]));
        grass->GetChild(0)->GetSpatial()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
        grass->GetChild(0)->GetSpatial()->SetShader(engine->shader_manager.GetShader(ShaderManager::Key::BASIC_VEGETATION).IncRef());
        grass->Scale(1.0f);
        grass->Translate({0, 1, 0});
        grass->GetChild(0)->GetSpatial()->AddController<AABBDebugController>(engine);


        material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_PARALLAX_HEIGHT, 0.1f);
        material_test_obj->Scale(3.45f);
        material_test_obj->Translate(Vector3(0, 22, 0));
        scene->GetRootNode()->AddChild(std::move(material_test_obj));

        // remove textures so we can manipulate the material and see our changes easier
        //material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_NORMAL_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ROUGHNESS_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_METALNESS_MAP, nullptr);
        
        auto cubemap = engine->resources.textures.Add(std::make_unique<TextureCube>(
           engine->assets.Load<Texture>(
               "textures/Lycksele3/posx.jpg",
               "textures/Lycksele3/negx.jpg",
               "textures/Lycksele3/posy.jpg",
               "textures/Lycksele3/negy.jpg",
               "textures/Lycksele3/posz.jpg",
               "textures/Lycksele3/negz.jpg"
            )
        ));
        cubemap->GetImage().SetIsSRGB(true);
        cubemap.Init();

        zombie->GetChild(0)->GetSpatial()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
        zombie->Scale(0.25f);
        zombie->Translate({0, 0, -5});
        zombie->GetChild(0)->GetSpatial()->GetController<AnimationController>()->Play(1.0f, LoopMode::REPEAT);
        zombie->GetChild(0)->GetSpatial()->AddController<AABBDebugController>(engine);
        scene->GetRootNode()->AddChild(std::move(zombie));
        //zombie->GetChild(0)->GetSpatial()->GetSkeleton()->FindBone("thigh.L")->SetLocalRotation(Quaternion({1.0f, 0.0f, 0.0f}, MathUtil::DegToRad(90.0f)));
        //zombie->GetChild(0)->GetSpatial()->GetSkeleton()->GetRootBone()->UpdateWorldTransform();

        auto my_light = engine->resources.lights.Add(std::make_unique<DirectionalLight>(
            Vector3(-0.5f, 0.5f, 0.0f).Normalize(),
            Vector4::One(),
            100000.0f
        ));
        scene->GetEnvironment()->AddLight(my_light.IncRef());

        m_point_light = engine->resources.lights.Add(std::make_unique<PointLight>(
            Vector3(2.0f, 4.0f, 0.0f),
            Vector4(1.0f, 0.3f, 0.1f, 1.0f),
            10000.0f,
            25.0f
        ));

        scene->GetEnvironment()->AddLight(m_point_light.IncRef());


        scene->GetEnvironment()->AddLight(engine->resources.lights.Add(std::make_unique<PointLight>(
            Vector3(-6.0f, 4.0f, 3.0f),
            Vector4(0.2f, 0.3f, 1.0f, 1.0f),
            4000.0f,
            5.0f
        )));
        scene->GetEnvironment()->AddLight(engine->resources.lights.Add(std::make_unique<PointLight>(
            Vector3(-3.0f, 12.0f, -4.0f),
            Vector4(1.0f, 1.0f, 1.0f, 1.0f),
            2000.0f,
            25.0f
        )));

        
        // scene->GetEnvironment()->AddRenderComponent<AABBRenderer>();


        /*test_model->Scale(40.0f);//14.075f);
        auto &terrain_material = test_model->GetChild(0)->GetSpatial()->GetMaterial();
        terrain_material->SetParameter(Material::MATERIAL_KEY_UV_SCALE, 50.0f);
        terrain_material->SetParameter(Material::MATERIAL_KEY_PARALLAX_HEIGHT, 0.08f);
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-albedo.png")));
        terrain_material->GetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP)->GetImage().SetIsSRGB(true);
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-normal-dx.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_AO_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-ao.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_PARALLAX_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1_Height.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1_Roughness.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_METALNESS_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-metallic.png")));
        test_model->Rotate(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)));*/

        test_model->Scale(0.15f);
        scene->GetRootNode()->AddChild(std::move(test_model));
        
        scene->GetEnvironment()->AddRenderComponent<ShadowRenderer>(
            my_light.IncRef(),
            Vector3::Zero(),
            50.0f
        );

        scene->GetEnvironment()->AddRenderComponent<CubemapRenderer>(
            renderer::Extent2D {128, 128},
            Vector3 {5, 8, 5},
            renderer::Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
        );

#if HYPERION_VK_TEST_VCT
        vct->SetParent(scene->GetEnvironment());
        vct->InitGame(engine); // temp
#endif
        // scene->GetEnvironment()->AddRenderComponent<VoxelConeTracing>(
        //     VoxelConeTracing::Params {
        //         BoundingBox(-64.0f, 64.0f)
        //     }
        // );


        tex1 = engine->resources.textures.Add(
            engine->assets.Load<Texture>("textures/dirt.jpg")
        );
        //tex1.Init();

        tex2 = engine->resources.textures.Add(
            engine->assets.Load<Texture>("textures/dummy.jpg")
        );
        //tex2.Init();

        cube_obj->Scale(50.0f);

        auto metal_material = engine->resources.materials.Add(std::make_unique<Material>());
        metal_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Material::Parameter(Vector4{ 1.0f, 0.5f, 0.2f, 1.0f }));
        metal_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, tex2.IncRef());
        metal_material.Init();

        auto skybox_material = engine->resources.materials.Add(std::make_unique<Material>());
        skybox_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Material::Parameter(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }));
        skybox_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, cubemap.IncRef());
        skybox_material.Init();

        auto &skybox_spatial = cube_obj->GetChild(0)->GetSpatial();
        skybox_spatial->SetMaterial(std::move(skybox_material));
        skybox_spatial->SetBucket(BUCKET_SKYBOX);
        skybox_spatial->SetShader(engine->shader_manager.GetShader(ShaderManager::Key::BASIC_SKYBOX).IncRef());
        skybox_spatial->SetMeshAttributes(
            FaceCullMode::FRONT,
            false,
            false
        );

        scene->AddSpatial(cube_obj->GetChild(0)->GetSpatial().IncRef());

        //test_model->GetChild(0)->GetSpatial()->SetMaterial(std::move(metal_material));

        //zombie->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, tex1.IncRef());

        //zombie->AddController<AudioController>(engine->assets.Load<AudioSource>("sounds/taunt.wav"));
        //zombie->GetController<AudioController>()->Play(1.0f, LoopMode::ONCE);

        //scene->GetRootNode()->AddController<BasicPagingController>(Extent3D{8, 8, 8}, Vector3::One());

        // if (auto my_script = engine->assets.Load<Script>("scripts/examples/example.hypscript")) {
        //     APIInstance api_instance;
        //     ScriptFunctions::Build(api_instance);

        //     if (my_script->Compile(api_instance)) {
        //         my_script->Bake();

        //         my_script->Decompile(&utf::cout);
    
        //         my_script->Run();

        //         for (int i = 0; i < 5; i++) {
        //             vm::Value args[] = { vm::Value(vm::Value::I32, {.i32 = i}) };

        //             my_script->CallFunction("OnTick", args, 1);
        //         }
        //     } else {
        //         /*DebugLog(LogType::Error, "Script error! %llu errors\n", my_script->GetErrors().Size());

        //         for (size_t i = 0; i < my_script->GetErrors().Size(); i++) {
        //             DebugLog(LogType::Error, "Error %llu: %s\n", i, my_script->GetErrors()[i].GetText().c_str());
        //         }*/
        //         my_script->GetErrors().WriteOutput(utf::cout);

        //         HYP_BREAKPOINT;
        //     }
        // }

        auto monkey = engine->assets.Load<Node>("models/monkey/monkey.obj");

        monkey->GetChild(0)->GetSpatial()->AddController<ScriptedController>(engine->assets.Load<Script>("scripts/examples/controller.hypscript"));
        // scene->AddSpatial(monkey->GetChild(0)->GetSpatial().IncRef());
        scene->GetRootNode()->AddChild(std::move(monkey));

    //     auto outline_pipeline = std::make_unique<GraphicsPipeline>(
    //         engine->shader_manager.GetShader(ShaderKey::STENCIL_OUTLINE).IncRef(),
    //         engine->GetRenderListContainer().Get(BUCKET_TRANSLUCENT).GetRenderPass().IncRef(),
    //         RenderableAttributeSet{
    //             .bucket            = BUCKET_TRANSLUCENT,
    //             .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes,
    //             .stencil_state     = {1, renderer::StencilMode::OUTLINE}
    //         }
    //     );
    //     outline_pipeline->SetBlendEnabled(true);
    //     auto outline_pipeline_ref = engine->AddGraphicsPipeline(std::move(outline_pipeline));

        

    //     outline_pipeline_ref.Init();
    }

    virtual void Teardown(Engine *engine) override
    {
        delete input_manager;

        Game::Teardown(engine);
    }

    virtual void OnFrameBegin(Engine *engine, Frame *frame) override
    {
        scene->GetEnvironment()->RenderComponents(engine, frame);

        engine->render_state.BindScene(scene);
    }

    virtual void OnFrameEnd(Engine *engine, Frame *) override
    {
        engine->render_state.UnbindScene();
    }

    virtual void Logic(Engine *engine, GameCounter::TickUnit delta) override
    {
        timer += delta;
        ++counter;

        engine->GetWorld().Update(engine, delta);

        //scene->Update(engine, delta);

        //scene->GetEnvironment()->GetShadowRenderer(0)->SetOrigin(scene->GetCamera()->GetTranslation());


        if (0) { // bad performance on large meshes. need bvh
        //if (input_manager->IsButtonDown(MOUSE_BUTTON_LEFT) && ray_cast_timer > 1.0f) {
        //    ray_cast_timer = 0.0f;
            const auto &mouse_position = input_manager->GetMousePosition();

            const auto mouse_x = mouse_position.x.load();
            const auto mouse_y = mouse_position.y.load();

            const auto mouse_world = scene->GetCamera()->TransformScreenToWorld(
                Vector2(
                    mouse_x / static_cast<float>(input_manager->GetWindow()->width),
                    mouse_y / static_cast<float>(input_manager->GetWindow()->height)
                )
            );

            auto ray_direction = mouse_world.Normalized() * -1.0f;

            Ray ray{scene->GetCamera()->GetTranslation(), Vector3(ray_direction)};
            RayTestResults results;

            if (engine->GetWorld().GetOctree().TestRay(ray, results)) {
                RayTestResults triangle_mesh_results;

                for (const auto &hit : results) {
                    // now ray test each result as triangle mesh to find exact hit point

                    if (auto lookup_result = engine->resources.spatials.Lookup(Spatial::ID{hit.id})) {
                        if (auto &mesh = lookup_result->GetMesh()) {
                            ray.TestTriangleList(
                                mesh->GetVertices(),
                                mesh->GetIndices(),
                                lookup_result->GetTransform(),
                                lookup_result->GetId().value,
                                triangle_mesh_results
                            );
                        }
                    }
                }

                if (!triangle_mesh_results.Empty()) {
                    const auto &mesh_hit = triangle_mesh_results.Front();

                    if (auto *sphere = scene->GetRootNode()->Select("sphere")) {
                        sphere->SetLocalTranslation(mesh_hit.hitpoint);
                    }
                }
            }
        }

        //ray_cast_timer += delta;
        
        //zombie->GetChild(0)->GetSpatial()->GetSkeleton()->FindBone("thigh.L")->SetLocalTranslation(Vector3(0, std::sin(timer * 0.3f), 0));
       // zombie->GetChild(0)->GetSpatial()->GetSkeleton()->FindBone("thigh.L")->SetLocalRotation(Quaternion({0, 1, 0}, timer * 0.35f));
        //zombie->GetChild(0)->GetSpatial()->GetSkeleton()->GetRootBone()->UpdateWorldTransform();

        // if (uint32_t(timer) % 2 == 0) {
        //     zombie->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, tex1.IncRef());
        // } else {
        //     zombie->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, tex2.IncRef());
        // }

        if (auto *suzanne = scene->GetRootNode()->Select("Suzanne")) {
            //suzanne->GetChild(0)->GetSpatial()->SetStencilAttributes(StencilState{ 1, renderer::StencilMode::FILL });

            //outline_pipeline_ref->AddSpatial(engine->resources.spatials.IncRef(suzanne->GetChild(0)->GetSpatial()));

            suzanne->SetLocalTranslation({7, std::sin(timer * 0.35f) * 7.0f + 7.0f, 5});

            //scene->GetCamera()->SetTarget(suzanne->GetWorldTranslation());
        }

        //m_point_light->SetPosition({ std::sin(timer * 0.5f) * 5.0f, 6.0f, std::cos(timer * 0.5f) * 5.0f });

         if (auto *sphere = scene->GetRootNode()->Select("sphere")) {
            //if (auto &material = sphere->GetChild(0)->GetSpatial()->GetMaterial()) {
            //    material->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, std::sin(timer * 0.5f) * 0.5f + 0.5f);
            //    material->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);////std::cos(timer) * 0.5f + 0.5f);
            //}
            //sphere->SetLocalTranslation(Vector3(7, 7, 3));
             sphere->SetLocalTranslation(scene->GetCamera()->GetTranslation() + scene->GetCamera()->GetDirection() * 15.0f);
         }
        
        // material_test_obj->SetLocalScale(3.45f);
        //material_test_obj->SetLocalRotation(Quaternion({0, 1, 0}, timer * 0.25f));
        // material_test_obj->SetLocalTranslation({16, 5.25f,12});
        //material_test_obj->SetLocalTranslation(Vector3(std::sin(timer * 0.5f) * 270, 0, std::cos(timer * 0.5f) * 270.0f) + Vector3(7, 3, 0));

        /*material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(
            std::sin(timer) * 0.5f + 0.5f,
            std::cos(timer) * 0.5f + 0.5f,
            ((std::sin(timer) * 0.5f + 0.5f) + (std::cos(timer) * 0.5f + 0.5f)) * 0.5f,
            1.0f
        ));*/

        // material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.75f);//std::sin(timer) * 0.5f + 0.5f);
        // material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.8f);//std::cos(timer) * 0.5f + 0.5f);
        // material_test_obj->GetChild(0)->GetSpatial()->Update(engine, delta);

        // zombie->Update(engine, delta);

    }

    
    InputManager *input_manager;

    Ref<Scene> scene;
    Ref<Texture> tex1, tex2;
    std::unique_ptr<Node> test_model, zombie, cube_obj, material_test_obj;
    GameCounter::TickUnit timer{};
    GameCounter::TickUnit ray_cast_timer{};
    std::atomic_uint32_t counter{0};

};
} // namespace hyperion::v2

int main()
{
    using namespace hyperion::renderer;
    
    SystemSDL system;
    SystemWindow *window = SystemSDL::CreateSystemWindow("Hyperion Engine", 1024, 768);
    system.SetCurrentWindow(window);

    SystemEvent event;

    auto *engine = new v2::Engine(system, "My app");

    engine->assets.SetBasePath(v2::FileSystem::Join(HYP_ROOT_DIR, "../res"));

    auto *my_game = new v2::MyGame;

#if HYPERION_VK_TEST_IMAGE_STORE
    renderer::Image *image_storage = new renderer::StorageImage(
        Extent3D{512, 512, 1},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );

    ImageView image_storage_view;
#endif
    
    engine->Initialize();

    Device *device = engine->GetInstance()->GetDevice();

#if HYPERION_VK_TEST_IMAGE_STORE
    descriptor_set_globals
        ->AddDescriptor<StorageImageDescriptor>(16)
        ->SetSubDescriptor({.image_view = &image_storage_view});

    HYPERION_ASSERT_RESULT(image_storage->Create(
        device,
        engine->GetInstance(),
        GPUMemory::ResourceState::UNORDERED_ACCESS
    ));

    HYPERION_ASSERT_RESULT(image_storage_view.Create(device, image_storage));
#endif

#if HYPERION_VK_TEST_SPARSE_VOXEL_OCTREE
    v2::SparseVoxelOctree svo;
    svo.Init(engine);
#endif

    engine->PrepareSwapchain();

    engine->shader_manager.SetShader(
        v2::ShaderKey::BASIC_VEGETATION,
        engine->resources.shaders.Add(std::make_unique<v2::Shader>(
            std::vector<v2::SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vegetation.vert.spv")).Read(),
                        {.name = "vegetation vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/forward_frag.spv")).Read(),
                        {.name = "forward frag"}
                    }
                }
            }
        ))
    );

    engine->shader_manager.SetShader(
        v2::ShaderKey::DEBUG_AABB,
        engine->resources.shaders.Add(std::make_unique<v2::Shader>(
            std::vector<v2::SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/aabb.vert.spv")).Read(),
                        {.name = "aabb vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/aabb.frag.spv")).Read(),
                        {.name = "aabb frag"}
                    }
                }
            }
        ))
    );

    engine->shader_manager.SetShader(
        v2::ShaderKey::BASIC_FORWARD,
        engine->resources.shaders.Add(std::make_unique<v2::Shader>(
            std::vector<v2::SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vert.spv")).Read(),
                        {.name = "main vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/forward_frag.spv")).Read(),
                        {.name = "forward frag"}
                    }
                }
            }
        ))
    );
    
    Frame *frame = nullptr;

#if HYPERION_VK_TEST_IMAGE_STORE
    /* Compute */
    auto compute_pipeline = engine->resources.compute_pipelines.Add(
        std::make_unique<v2::ComputePipeline>(
            engine->resources.shaders.Add(std::make_unique<v2::Shader>(
                std::vector<v2::SubShader>{
                    { ShaderModule::Type::COMPUTE, {FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/imagestore.comp.spv")).Read()}}
                }
            ))
        )
    );

    compute_pipeline.Init();

    auto compute_command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::Type::COMMAND_BUFFER_PRIMARY);

    SemaphoreChain compute_semaphore_chain({}, { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT });
    AssertThrow(compute_semaphore_chain.Create(engine->GetInstance()->GetDevice()));

    HYPERION_ASSERT_RESULT(compute_command_buffer->Create(engine->GetInstance()->GetDevice(), engine->GetInstance()->GetComputeCommandPool()));
    
    for (size_t i = 0; i < engine->GetInstance()->GetFrameHandler()->NumFrames(); i++) {
        /* Wait for compute to finish */
        compute_semaphore_chain.SignalsTo(engine->GetInstance()->GetFrameHandler()
            ->GetPerFrameData()[i].Get<Frame>()
            ->GetPresentSemaphores());
    }
#endif
    

    PerFrameData<CommandBuffer, Semaphore> per_frame_data(engine->GetInstance()->GetFrameHandler()->NumFrames());

    for (uint32_t i = 0; i < per_frame_data.NumFrames(); i++) {
        auto cmd_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);
        AssertThrow(cmd_buffer->Create(engine->GetInstance()->GetDevice(), engine->GetInstance()->GetGraphicsQueue().command_pool));

        per_frame_data[i].Set<CommandBuffer>(std::move(cmd_buffer));
    }

    
    // engine->shader_manager.SetShader(
    //     v2::ShaderManager::Key::STENCIL_OUTLINE,
    //     engine->resources.shaders.Add(std::make_unique<v2::Shader>(
    //         std::vector<v2::SubShader>{
    //             {ShaderModule::Type::VERTEX, {FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/outline.vert.spv")).Read()}},
    //             {ShaderModule::Type::FRAGMENT, {FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/outline.frag.spv")).Read()}}
    //         }
    //     ))
    // );


    {
        engine->shader_manager.SetShader(
            v2::ShaderManager::Key::BASIC_SKYBOX,
            engine->resources.shaders.Add(std::make_unique<v2::Shader>(
                std::vector<v2::SubShader>{
                    {ShaderModule::Type::VERTEX, {FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/skybox_vert.spv")).Read()}},
                    {ShaderModule::Type::FRAGMENT, {FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/skybox_frag.spv")).Read()}}
                }
            ))
        );

        /*auto pipeline = std::make_unique<v2::GraphicsPipeline>(
            engine->shader_manager.GetShader(v2::ShaderManager::Key::BASIC_SKYBOX).IncRef(),
            engine->GetRenderListContainer().Get(v2::BUCKET_SKYBOX).GetRenderPass().IncRef(),
            renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes,
            v2::Bucket::BUCKET_SKYBOX
        );
        pipeline->SetFaceCullMode(FaceCullMode::FRONT);
        pipeline->SetDepthTest(false);
        pipeline->SetDepthWrite(false);
        
        engine->AddGraphicsPipeline(std::move(pipeline));*/
    }
    
    {
        auto pipeline = std::make_unique<v2::GraphicsPipeline>(
            engine->shader_manager.GetShader(v2::ShaderManager::Key::BASIC_FORWARD).IncRef(),
            engine->GetRenderListContainer().Get(v2::BUCKET_TRANSLUCENT).GetRenderPass().IncRef(),
            v2::RenderableAttributeSet{
                .bucket            = v2::Bucket::BUCKET_TRANSLUCENT,
                .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
            }
        );
        pipeline->SetBlendEnabled(true);
        
        engine->AddGraphicsPipeline(std::move(pipeline));
    }
    

    my_game->Init(engine, window);

#if HYPERION_VK_TEST_VCT
    vct = new v2::VoxelConeTracing({
        /* scene bounds for vct to capture */
        .aabb = BoundingBox(Vector3(-64), Vector3(64))
    });

    vct->Init(engine);
#endif


#if HYPERION_VK_TEST_RAYTRACING
    auto rt_shader = std::make_unique<ShaderProgram>();
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_GEN, {
        FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/rt/test.rgen.spv")).Read()
    });
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_MISS, {
        FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/rt/test.rmiss.spv")).Read()
    });
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_CLOSEST_HIT, {
        FileByteReader(v2::FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/rt/test.rchit.spv")).Read()
    });

    auto rt = std::make_unique<RaytracingPipeline>(std::move(rt_shader));

    my_game->material_test_obj->GetChild(0)->GetSpatial()->SetTransform({{ 0, 7, 0 }});


    v2::ProbeGrid probe_system({
        .aabb = {{-20.0f, -5.0f, -20.0f}, {20.0f, 5.0f, 20.0f}}
    });
    probe_system.Init(engine);

    auto my_tlas = std::make_unique<v2::Tlas>();

    my_tlas->AddBlas(engine->resources.blas.Add(std::make_unique<v2::Blas>(
        engine->resources.meshes.IncRef(my_game->material_test_obj->GetChild(0)->GetSpatial()->GetMesh()),
        my_game->material_test_obj->GetChild(0)->GetSpatial()->GetTransform()
    )));
    
    my_tlas->AddBlas(engine->resources.blas.Add(std::make_unique<v2::Blas>(
        engine->resources.meshes.IncRef(my_game->cube_obj->GetChild(0)->GetSpatial()->GetMesh()),
        my_game->cube_obj->GetChild(0)->GetSpatial()->GetTransform()
    )));

    my_tlas->Init(engine);
    
    Image *rt_image_storage = new StorageImage(
        Extent3D{1024, 1024, 1},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );

    ImageView rt_image_storage_view;

    auto *rt_descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_RAYTRACING);
    rt_descriptor_set->AddDescriptor<TlasDescriptor>(0)
        ->SetSubDescriptor({.acceleration_structure = &my_tlas->Get()});
    rt_descriptor_set->AddDescriptor<StorageImageDescriptor>(1)
        ->SetSubDescriptor({.image_view = &rt_image_storage_view});
    
    auto rt_storage_buffer = rt_descriptor_set->AddDescriptor<StorageBufferDescriptor>(3);
    rt_storage_buffer->SetSubDescriptor({.buffer = my_tlas->Get().GetMeshDescriptionsBuffer()});

    HYPERION_ASSERT_RESULT(rt_image_storage->Create(
        device,
        engine->GetInstance(),
        GPUMemory::ResourceState::UNORDERED_ACCESS
    ));
    HYPERION_ASSERT_RESULT(rt_image_storage_view.Create(engine->GetDevice(), rt_image_storage));
#endif

    engine->Compile();

#if HYPERION_VK_TEST_RAYTRACING
    HYPERION_ASSERT_RESULT(rt->Create(engine->GetDevice(), &engine->GetInstance()->GetDescriptorPool()));
#endif

#if HYPERION_RUN_TESTS
    AssertThrow(test::GlobalTestManager::PrintReport(test::GlobalTestManager::Instance()->RunAll()));
#endif


#if HYPERION_VK_TEST_SPARSE_VOXEL_OCTREE
    svo.Build(engine);
#endif

#if HYPERION_VK_TEST_IMAGE_STORE
    auto compute_fc = std::make_unique<Fence>(true);
    HYPERION_ASSERT_RESULT(compute_fc->Create(engine->GetDevice()));
#endif

    engine->game_thread.Start(engine, my_game, window);

    bool running = true;

    float tmp_render_timer = 0.0f;

    UInt num_frames = 0;
    float delta_time_accum = 0.0f;
    GameCounter counter;

    while (running) {
        while (SystemSDL::PollEvent(&event)) {
            my_game->input_manager->CheckEvent(&event);
            switch (event.GetType()) {
                case SystemEventType::EVENT_SHUTDOWN:
                    running = false;
                    break;
                case SystemEventType::EVENT_MOUSESCROLL:
                {
                    if (my_game->scene != nullptr) {
                        int wheel_x, wheel_y;
                        event.GetMouseWheel(&wheel_x, &wheel_y);

                        my_game->scene->GetCamera()->PushCommand(CameraCommand {
                            .command = CameraCommand::CAMERA_COMMAND_SCROLL,
                            .scroll_data = {
                                .wheel_x = wheel_x,
                                .wheel_y = wheel_y
                            }
                        });
                    }

                    break;
                }
                case SystemEventType::EVENT_MOUSEMOTION:
                {
                    const auto &mouse_position = my_game->input_manager->GetMousePosition();

                    int mouse_x = mouse_position.x.load(),
                        mouse_y = mouse_position.y.load();

                    float mx, my;

                    int window_width, window_height;
                    my_game->input_manager->GetWindow()->GetSize(&window_width, &window_height);

                    mx = (static_cast<float>(mouse_x) - static_cast<float>(window_width) * 0.5f) / (static_cast<float>(window_width));
                    my = (static_cast<float>(mouse_y) - static_cast<float>(window_height) * 0.5f) / (static_cast<float>(window_height));
                    
                    if (my_game->scene != nullptr) {
                        my_game->scene->GetCamera()->PushCommand(CameraCommand {
                            .command = CameraCommand::CAMERA_COMMAND_MAG,
                            .mag_data = {
                                .mouse_x = mouse_x,
                                .mouse_y = mouse_y,
                                .mx      = mx,
                                .my      = my
                            }
                        });
                    }

                    break;
                }
                default:
                    break;
            }
        }

        if (my_game->input_manager->IsKeyDown(KEY_W)) {
            if (my_game->scene != nullptr) {
                my_game->scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_FORWARD,
                        .amount        = 1.0f
                    }
                });
            }
        }
        if (my_game->input_manager->IsKeyDown(KEY_S)) {
            if (my_game->scene != nullptr) {
                my_game->scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_BACKWARD,
                        .amount        = 1.0f
                    }
                });
            }
        }
        if (my_game->input_manager->IsKeyDown(KEY_A)) {
            if (my_game->scene != nullptr) {
                my_game->scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_LEFT,
                        .amount        = 1.0f
                    }
                });
            }
        }
        if (my_game->input_manager->IsKeyDown(KEY_D)) {
            if (my_game->scene != nullptr) {
                my_game->scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_RIGHT,
                        .amount        = 1.0f
                    }
                });
            }
        }

        counter.NextTick();
        delta_time_accum += counter.delta;
        num_frames++;

        if (num_frames >= 1000) {
            DebugLog(
                LogType::Debug,
                "Render FPS: %f\n",
                1.0f / (delta_time_accum / static_cast<float>(num_frames))
            );

            delta_time_accum = 0.0f;
            num_frames = 0;
        }

        HYPERION_ASSERT_RESULT(engine->GetInstance()->GetFrameHandler()->PrepareFrame(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetSwapchain()
        ));

        frame = engine->GetInstance()->GetFrameHandler()->GetCurrentFrameData().Get<Frame>();
        auto *command_buffer = frame->GetCommandBuffer();
        const uint32_t frame_index = engine->GetInstance()->GetFrameHandler()->GetCurrentFrameIndex();

        engine->GetRenderListContainer().AddPendingGraphicsPipelines(engine);



        if (auto num_enqueued = engine->render_scheduler.NumEnqueued()) {
            engine->render_scheduler.Flush([command_buffer, frame_index](v2::RenderFunctor &fn) {
                HYPERION_ASSERT_RESULT(fn(command_buffer, frame_index));
            });

            /*DebugLog(
                LogType::Debug,
                "[Renderer] Execute %lu enqueued tasks\n",
                num_enqueued
            );*/
        }

#if HYPERION_VK_TEST_IMAGE_STORE
        compute_pipeline->GetPipeline()->push_constants = {
            .counter = {
                .x = static_cast<uint32_t>(std::sin(0.0f) * 20.0f),
                .y = static_cast<uint32_t>(std::cos(0.0f) * 20.0f)
            }
        };

        /* Compute */
        HYPERION_ASSERT_RESULT(compute_fc->WaitForGpu(engine->GetDevice()));
        HYPERION_ASSERT_RESULT(compute_fc->Reset(engine->GetDevice()));
        
        HYPERION_ASSERT_RESULT(compute_command_buffer->Reset(engine->GetInstance()->GetDevice()));
        HYPERION_ASSERT_RESULT(compute_command_buffer->Record(engine->GetInstance()->GetDevice(), nullptr, [&](CommandBuffer *cmd) {
            compute_pipeline->GetPipeline()->Bind(cmd);

            engine->GetInstance()->GetDescriptorPool().Bind(
                engine->GetInstance()->GetDevice(),
                cmd,
                compute_pipeline->GetPipeline(),
                {{
                    .set = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL,
                    .count = 1
                }}
            );

            compute_pipeline->GetPipeline()->Dispatch(cmd, {8, 8, 1});

            HYPERION_RETURN_OK;
        }));

        HYPERION_ASSERT_RESULT(compute_command_buffer->SubmitPrimary(
            engine->GetInstance()->GetComputeQueue().queue,
            compute_fc.get(),
            &compute_semaphore_chain
        ));
#endif

        engine->UpdateBuffersAndDescriptors(frame_index);
        engine->ResetRenderState();

        /* === rendering === */
        HYPERION_ASSERT_RESULT(frame->BeginCapture(engine->GetInstance()->GetDevice()));

        my_game->OnFrameBegin(engine, frame);

#if HYPERION_VK_TEST_RAYTRACING
        rt->Bind(frame->GetCommandBuffer());
        engine->GetInstance()->GetDescriptorPool().Bind(
            engine->GetDevice(),
            frame->GetCommandBuffer(),
            rt.get(),
            {
                {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE, .count = 1},
                {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
                {.offsets = {0, 0}}
            }
        );
        engine->GetInstance()->GetDescriptorPool().Bind(
            engine->GetDevice(),
            frame->GetCommandBuffer(),
            rt.get(),
            {{
                .set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
                .count = 1
            }}
        );
        rt->TraceRays(engine->GetDevice(), frame->GetCommandBuffer(), rt_image_storage->GetExtent());
        rt_image_storage->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );

        
        probe_system.RenderProbes(engine, frame->GetCommandBuffer());
        probe_system.ComputeIrradiance(engine, frame->GetCommandBuffer());
#endif

#if HYPERION_VK_TEST_VCT
        if (tmp_render_timer <= 0.0f || tmp_render_timer > 0.002f) {
            vct->OnRender(engine, frame);
           tmp_render_timer = 0.001f;
        }
        tmp_render_timer += 0.001f;
#endif

        engine->RenderDeferred(frame);
        
#if HYPERION_VK_TEST_IMAGE_STORE
        image_storage->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );
#endif

        engine->RenderFinalPass(frame);

        HYPERION_ASSERT_RESULT(frame->EndCapture(engine->GetInstance()->GetDevice()));
        HYPERION_ASSERT_RESULT(frame->Submit(&engine->GetInstance()->GetGraphicsQueue()));
        
        my_game->OnFrameEnd(engine, frame);

        engine->GetInstance()->GetFrameHandler()->PresentFrame(&engine->GetInstance()->GetGraphicsQueue(), engine->GetInstance()->GetSwapchain());
        engine->GetInstance()->GetFrameHandler()->NextFrame();

    }

    AssertThrow(engine->GetInstance()->GetDevice()->Wait());

    v2::FullScreenPass::full_screen_quad.reset();// have to do this here for now or else buffer does not get cleared before device is deleted

#if HYPERION_VK_TEST_IMAGE_STORE
    HYPERION_ASSERT_RESULT(image_storage_view.Destroy(device));
    HYPERION_ASSERT_RESULT(image_storage->Destroy(device));

    delete image_storage;

#endif

    for (size_t i = 0; i < per_frame_data.NumFrames(); i++) {
        per_frame_data[i].Get<CommandBuffer>()->Destroy(engine->GetInstance()->GetDevice(), engine->GetInstance()->GetGraphicsCommandPool());
    }
    per_frame_data.Reset();

#if HYPERION_VK_TEST_IMAGE_STORE
    compute_command_buffer->Destroy(device, engine->GetInstance()->GetComputeCommandPool());
    compute_semaphore_chain.Destroy(engine->GetInstance()->GetDevice());

    compute_fc->Destroy(engine->GetDevice());
#endif

#if HYPERION_VK_TEST_RAYTRACING

    rt_image_storage->Destroy(engine->GetDevice());
    rt_image_storage_view.Destroy(engine->GetDevice());
    //tlas->Destroy(engine->GetInstance());
    rt->Destroy(engine->GetDevice());
#endif


    engine->terrain_thread.Stop();
    engine->terrain_thread.Join();

    engine->m_running = false;

    engine->game_thread.Join();

    delete vct;

    delete my_game;

    delete engine;
    delete window;

    return 0;
}
