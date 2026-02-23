#include "util/tile_util.wgsl"

struct AvalancheAnimationSettings {
    output_resolution: vec2<u32>,
    region_min: vec2<f32>,
    region_size: vec2<f32>,
    min_slope_angle: f32,
    max_slope_angle: f32,
    sampling_interval: vec2<u32>,
    num_particles_per_cell: u32,
    _padding: u32,
};

@group(0) @binding(0) var<uniform> settings: AvalancheAnimationSettings;
@group(0) @binding(1) var normal_texture: texture_2d<f32>;
@group(0) @binding(2) var height_texture: texture_2d<f32>;
@group(0) @binding(3) var release_texture: texture_2d<f32>;
@group(0) @binding(4) var<storage, read_write> output_buffer: array<vec4f>;

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) gid: vec3<u32>) {
    let dims = textureDimensions(normal_texture);
    let width = dims.x;
    let height_dim = dims.y;
    if (gid.x >= width || gid.y >= height_dim) {
        return;
    }

    let idx = gid.y * width + gid.x;
    let dims_f = vec2f(dims);
    let denom = max(vec2f(1.0, 1.0), dims_f - vec2f(1.0, 1.0));
    let uv = vec2f(gid.xy) / denom;
    // TileStitchNode flips y, so y=0 is north; flip to map into region bounds correctly.
    let world_xy = settings.region_min + vec2f(uv.x, 1.0 - uv.y) * settings.region_size;
    let height_value = textureLoad(height_texture, vec2<i32>(gid.xy), 0).x;
    let altitude_correction_factor = 1.0 / cos(y_to_lat(world_xy.y));
    let world_z = height_value * altitude_correction_factor;
    output_buffer[idx] = vec4f(world_xy, world_z, height_value);
}
