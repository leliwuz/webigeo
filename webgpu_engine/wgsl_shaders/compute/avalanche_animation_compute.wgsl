#include "util/tile_util.wgsl"
#include "util/normals_util.wgsl"
#include "util/hashing.wgsl"

struct AvalancheAnimationSettings {
    output_resolution: vec2<u32>,
    region_min: vec2<f32>,
    region_size: vec2<f32>,
    min_slope_angle: f32,
    max_slope_angle: f32,
    sampling_interval: vec2<u32>,
    num_particles_per_cell: u32,
};

struct OutputCount {
    value: atomic<u32>,
};

@group(0) @binding(0) var<uniform> settings: AvalancheAnimationSettings;
@group(0) @binding(1) var normal_texture: texture_2d<f32>;
@group(0) @binding(2) var height_texture: texture_2d<f32>;
@group(0) @binding(3) var release_texture: texture_2d<f32>;
@group(0) @binding(4) var<storage, read_write> output_buffer: array<vec4f>;
@group(0) @binding(5) var<storage, read_write> output_count: OutputCount;

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) gid: vec3<u32>) {
    let dims = textureDimensions(normal_texture);
    let width = dims.x;
    let height_dim = dims.y;
    if (gid.x >= width || gid.y >= height_dim) {
        return;
    }
    if (gid.z >= settings.num_particles_per_cell) {
        return;
    }

    let dims_f = vec2f(dims);
    let denom = max(vec2f(1.0, 1.0), dims_f - vec2f(1.0, 1.0));
    let uv = vec2f(gid.xy) / denom;
    let cell_size = settings.region_size / denom;
    // TileStitchNode flips y, so y=0 is north; flip to map into region bounds correctly.
    let world_xy = settings.region_min + vec2f(uv.x, 1.0 - uv.y) * settings.region_size;
    if ((gid.x % settings.sampling_interval.x) != 0u || (gid.y % settings.sampling_interval.y) != 0u) {
        return;
    }
    let normal = textureLoad(normal_texture, vec2<i32>(gid.xy), 0).xyz * 2.0 - 1.0;
    let slope_angle = get_slope_angle(normal);
    if (slope_angle < settings.min_slope_angle || slope_angle > settings.max_slope_angle) {
        return;
    }
    let release_value = textureLoad(release_texture, vec2<i32>(gid.xy), 0);
    if (release_value.a <= 0.5) {
        return;
    }
    let height_value = textureLoad(height_texture, vec2<i32>(gid.xy), 0).x;
    let altitude_correction_factor = 1.0 / cos(y_to_lat(world_xy.y));
    let world_z = height_value * altitude_correction_factor;
    let max_count = settings.output_resolution.x * settings.output_resolution.y * settings.num_particles_per_cell;
    let write_index = atomicAdd(&output_count.value, 1u);
    if (write_index >= max_count) {
        return;
    }
    let jitter_hash = compute_hash(gid.x + gid.y * width + gid.z * 73856093u);
    let jitter_xy = vec2f(
        f32(jitter_hash & 0xFFFFu) / 65535.0,
        f32((jitter_hash >> 16u) & 0xFFFFu) / 65535.0);
    let jitter = (jitter_xy - vec2f(0.5, 0.5)) * (cell_size * 0.9);
    output_buffer[write_index] = vec4f(world_xy + jitter, world_z, height_value);
}
