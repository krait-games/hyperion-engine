#include "./framebuffer_cube.h"
#include "../opengl.h"
#include "../gl_util.h"

#include <iostream>

namespace apex {

FramebufferCube::FramebufferCube(int width, int height)
    : Framebuffer(width, height)
{
    const int num_components = 3; // r, g, b
    const size_t color_map_texture_byte_size = width * height * num_components,
        depth_map_texture_byte_size = width * height;

    std::array<std::shared_ptr<Texture2D>, 6> color_textures, depth_textures;

    for (int i = 0; i < color_textures.size(); i++) {
        unsigned char *data = (unsigned char*)malloc(color_map_texture_byte_size);
        memset(data, 0, color_map_texture_byte_size);

        color_textures[i] = std::make_shared<Texture2D>(width, height, data);
    }

    color_texture = std::make_shared<Cubemap>(color_textures);
    color_texture->SetInternalFormat(GL_RGB8);
    color_texture->SetFormat(GL_RGB);
    color_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    color_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    for (int i = 0; i < depth_textures.size(); i++) {
        unsigned char *data = (unsigned char*)malloc(depth_map_texture_byte_size);
        memset(data, 0, depth_map_texture_byte_size);

        depth_textures[i] = std::make_shared<Texture2D>(width, height, data);
        depth_textures[i]->SetInternalFormat(GL_DEPTH_COMPONENT24);
        depth_textures[i]->SetFormat(GL_DEPTH_COMPONENT);
    }

    depth_texture = std::make_shared<Cubemap>(depth_textures);
    depth_texture->SetInternalFormat(GL_DEPTH_COMPONENT24);
    depth_texture->SetFormat(GL_DEPTH_COMPONENT);
    depth_texture->SetFilter(GL_NEAREST, GL_NEAREST);
    depth_texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
}

FramebufferCube::~FramebufferCube()
{
}

const std::shared_ptr<Texture> FramebufferCube::GetColorTexture() const { return color_texture; }
const std::shared_ptr<Texture> FramebufferCube::GetNormalTexture() const { return nullptr; }
const std::shared_ptr<Texture> FramebufferCube::GetPositionTexture() const { return nullptr; }
const std::shared_ptr<Texture> FramebufferCube::GetDepthTexture() const { return depth_texture; }
const std::shared_ptr<Texture> FramebufferCube::GetDataTexture() const { return nullptr; }

void FramebufferCube::Use()
{
    if (!is_created) {
        glGenFramebuffers(1, &id);
        is_created = true;

        CatchGLErrors("Failed to generate framebuffer for framebuffer cube.");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, id);
    glViewport(0, 0, width, height);

    if (!is_uploaded) {
        color_texture->Use();
        //glFramebufferTexture(GL_FRAMEBUFFER,
        //    GL_COLOR_ATTACHMENT0, color_texture->GetId(), 0);
        for (int i = 0; /*i < 6*/ i < 1; i++) {
            glFramebufferTexture(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0, color_texture->GetId(), 0);

            CatchGLErrors("Failed to set framebuffer cube color data");
        }

        color_texture->End();

        depth_texture->Use();
        for (int i = 0; /*i < 6*/ i < 1; i++) {
            glFramebufferTexture(GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT, depth_texture->GetId(), 0);

            CatchGLErrors("Failed to set framebuffer cube depth data");
        }
        depth_texture->End();

        const unsigned int draw_buffers[] = {
            GL_COLOR_ATTACHMENT0, // color map
           // GL_COLOR_ATTACHMENT1
        };

        glDrawBuffers(1, draw_buffers);
        CatchGLErrors("Failed to use glDrawBuffers in framebuffer cube");

        unsigned int status;

        if ((status = glCheckFramebufferStatus(GL_FRAMEBUFFER)) != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "Could not create FramebufferCube " << status << std::endl;
            throw std::runtime_error("Could not create FramebufferCube");
        }

        is_uploaded = true;
    }

}

void FramebufferCube::Store(const std::shared_ptr<Texture> &texture, int index)
{
    // not implemented
}

} // namespace apex
