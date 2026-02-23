#include "util/tile_util.wgsl"

struct ParticleStepSettings {
    region_min: vec2f,
    region_size: vec2f,
    output_resolution: vec2u,
    dt: f32,
    gravity: f32,
};

struct OutputCount {
    value: atomic<u32>,
};

@group(0) @binding(0) var<uniform> settings: ParticleStepSettings;
@group(0) @binding(1) var normal_texture: texture_2d<f32>;
@group(0) @binding(2) var height_texture: texture_2d<f32>;
@group(0) @binding(3) var<storage, read_write> positions: array<vec4f>;
@group(0) @binding(4) var<storage, read_write> velocities: array<vec4f>;
@group(0) @binding(5) var<storage, read_write> output_count: OutputCount;

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) gid: vec3<u32>) {
    let idx = gid.x;
    let count = atomicLoad(&output_count.value);
    if (idx >= count) {
        return;
    }

    var pos = positions[idx].xyz;
    var vel = velocities[idx].xyz;

    let uv = (pos.xy - settings.region_min) / settings.region_size;
    let uv_y = 1.0 - uv.y;
    if (uv.x < 0.0 || uv.x > 1.0 || uv_y < 0.0 || uv_y > 1.0) {
        return;
    }

    let tex_size = textureDimensions(normal_texture);
    let tex_pos = vec2<i32>(clamp(vec2f(uv.x, uv_y) * vec2f(tex_size - 1), vec2f(0.0), vec2f(tex_size - 1)));

    let normal_raw = textureLoad(normal_texture, tex_pos, 0).xyz;
    let normal = normalize(normal_raw * 2.0 - 1.0);

    let gravity_vec = vec3f(0.0, 0.0, -settings.gravity);
    let accel = gravity_vec - dot(gravity_vec, normal) * normal;

    vel = vel + accel * settings.dt;
    vel = vel - dot(vel, normal) * normal;
    pos = pos + vel * settings.dt;

    let height_value = textureLoad(height_texture, tex_pos, 0).x;
    let altitude_correction_factor = 1.0 / cos(y_to_lat(pos.y));
    pos.z = height_value * altitude_correction_factor;

    positions[idx] = vec4f(pos, 1.0);
    velocities[idx] = vec4f(vel, 0.0);
}
