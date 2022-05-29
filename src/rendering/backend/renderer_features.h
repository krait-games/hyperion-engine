#ifndef HYPERION_V2_BACKEND_RENDERER_FEATURES_H
#define HYPERION_V2_BACKEND_RENDERER_FEATURES_H

#include <util/defines.h>

//#define HYP_FEATURES_ENABLE_RAYTRACING 1

#if HYP_VULKAN
#include <rendering/backend/vulkan/renderer_features.h>
#else
#error Unsupported rendering backend
#endif

#endif