#include "texture.h"
#include "../engine.h"

namespace hyperion::v2 {

Texture::Texture(
    Extent3D extent,
    Image::InternalFormat format,
    Image::Type type,
    Image::FilterMode filter_mode,
    Image::WrapMode wrap_mode,
    const unsigned char *bytes)
    : Texture(
        TextureImage(
            extent,
            format,
            type,
            filter_mode,
            bytes
        ),
        filter_mode,
        wrap_mode
    )
{
}

Texture::Texture(
    Image &&image,
    Image::FilterMode filter_mode,
    Image::WrapMode wrap_mode
) : EngineComponentBase(),
    m_image(std::move(image)),
    m_image_view(),
    m_sampler(filter_mode, wrap_mode)
{
}

Texture::~Texture()
{
    Teardown();
}

void Texture::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_TEXTURES, [this](Engine *engine) {
        engine->render_scheduler.Enqueue([this, engine](...) {
            const auto initial_state = renderer::GPUMemory::ResourceState::SHADER_RESOURCE;

            HYPERION_BUBBLE_ERRORS(m_image.Create(engine->GetDevice(), engine->GetInstance(), initial_state));
            HYPERION_BUBBLE_ERRORS(m_image_view.Create(engine->GetInstance()->GetDevice(), &m_image));
            HYPERION_BUBBLE_ERRORS(m_sampler.Create(engine->GetInstance()->GetDevice(), &m_image_view));

            engine->shader_globals->textures.AddResource(engine->resources.textures.IncRef(this));

            HYPERION_RETURN_OK;
        });
        
        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_TEXTURES, [this](Engine *engine) {
            engine->render_scheduler.Enqueue([this, engine](...) {
                engine->shader_globals->textures.RemoveResource(m_id);

                HYPERION_BUBBLE_ERRORS(m_sampler.Destroy(engine->GetInstance()->GetDevice()));
                HYPERION_BUBBLE_ERRORS(m_image_view.Destroy(engine->GetInstance()->GetDevice()));
                HYPERION_BUBBLE_ERRORS(m_image.Destroy(engine->GetInstance()->GetDevice()));

                HYPERION_RETURN_OK;
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        }), engine);
    }));
}

} // namespace hyperion::v2