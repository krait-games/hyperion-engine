#include "noise_terrain_chunk.h"
#include "../../rendering/shader_manager.h"
#include "../../rendering/shaders/lighting_shader.h"
#include "../../rendering/environment.h"
#include "worley_noise.h"

#include <noise/noise.h>
#include <noise/module/ridgedmulti.h>
using namespace noise;

#include "open_simplex_noise.h"

#define MOUNTAIN_SCALE_WIDTH 0.02
#define MOUNTAIN_SCALE_LENGTH 0.02
#define MOUNTAIN_SCALE_HEIGHT 6.0

#define ROUGH_SCALE_WIDTH 0.8
#define ROUGH_SCALE_LENGTH 0.8
#define ROUGH_SCALE_HEIGHT 1.3

#define SMOOTH_SCALE_WIDTH 0.08
#define SMOOTH_SCALE_LENGTH 0.08
#define SMOOTH_SCALE_HEIGHT 1.0

#define MASK_SCALE_WIDTH 0.02
#define MASK_SCALE_LENGTH 0.02

namespace apex {
NoiseTerrainChunk::NoiseTerrainChunk(const HeightInfo &height_info, int seed)
    : TerrainChunk(height_info)
{
    module::RidgedMulti multi;
    multi.SetSeed(seed);
    multi.SetFrequency(0.03);
    multi.SetNoiseQuality(NoiseQuality::QUALITY_FAST);
    multi.SetOctaveCount(11);
    multi.SetLacunarity(2.0);

    WorleyNoise worley(seed);

    module::Perlin maskgen;
    maskgen.SetFrequency(0.05);
    maskgen.SetPersistence(0.25);

    struct osn_context *ctx;
    open_simplex_noise(seed, &ctx);

    heights.resize((height_info.width) * (height_info.length));

    for (int z = 0; z < height_info.length; z++) {
        for (int x = 0; x < height_info.width; x++) {
            int x_offset = x + (height_info.position.x * (height_info.width - 1));
            int z_offset = z + (height_info.position.y * (height_info.length - 1));

            //double smooth = (open_simplex_noise2(ctx, x_offset * SMOOTH_SCALE_WIDTH, 
            //    z_offset * SMOOTH_SCALE_HEIGHT) * 2.0 - 1.0) * SMOOTH_SCALE_HEIGHT;

            //double mask = maskgen.GetValue(x_offset * MASK_SCALE_WIDTH,
            //    z_offset * MASK_SCALE_LENGTH, 0.0);

            double rough = (multi.GetValue(x_offset * ROUGH_SCALE_WIDTH,
                z_offset * ROUGH_SCALE_LENGTH, 0.0) * 2.0 - 1.0) * ROUGH_SCALE_HEIGHT;

            double mountain = (worley.Noise(x_offset * MOUNTAIN_SCALE_WIDTH,
                z_offset * MOUNTAIN_SCALE_LENGTH, 0.0) * 2.0 - 1.0) * MOUNTAIN_SCALE_HEIGHT;

            heights[HeightIndexAt(x, z)] = rough + mountain;
        }
    }

    open_simplex_noise_free(ctx);

    auto mesh = BuildMesh(heights);
    mesh->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>({
        { "SHADOWS", Environment::GetInstance()->ShadowsEnabled() },
        { "NUM_SPLITS", Environment::GetInstance()->NumCascades() }
    }));
    entity = std::make_shared<Entity>("terrain_node");
    entity->SetRenderable(mesh);
}

int NoiseTerrainChunk::HeightIndexAt(int x, int z)
{
    int size = height_info.width;
    return ((x + size) % size) + ((z + size) % size) * size;
}

/*double fbm(Vector3 point, double H, double lacunarity, double octaves)
{
    double value, frequency, remainder, Noise3(Vector3);
    int i;
    static bool first = true;
    static double *exponent_array;

    if (first) {
        exponent_array = new double[octaves + 1];
        frequency = 1.0;

        for (i = 0; i < octaves; i++) {
            exponent_array[i] = pow(frequency, -H);
            frequency *= lacunarity;
        }
        first = false;
    }

    value = 0.0;
    frequency = 1.0;

    for (i = 0; i < octaves; i++) {
        value += Noise3(point) * exponent_array[i];
        point *= lacunarity;
    }

    remainder = octaves - (int)octaves;
    if (remainder) {
        value += remainder * Noise3(point) * exponent_array[i];
    }
    return value;
}

double multifractal(Vector3 point, double H, double lacunarity, double octaves, double offset)
{
}*/
}