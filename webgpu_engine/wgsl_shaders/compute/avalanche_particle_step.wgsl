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

struct DrawIndirectArgs {
    vertex_count: u32,
    instance_count: u32,
    first_vertex: u32,
    first_instance: u32,
};

@group(0) @binding(0) var<uniform> settings: ParticleStepSettings;
@group(0) @binding(1) var normal_texture: texture_2d<f32>;
@group(0) @binding(2) var height_texture: texture_2d<f32>;
@group(0) @binding(3) var<storage, read_write> positions: array<vec4f>;
@group(0) @binding(4) var<storage, read_write> velocities: array<vec4f>;
@group(0) @binding(5) var<storage, read_write> output_count: OutputCount;
@group(0) @binding(6) var<storage, read_write> draw_args: DrawIndirectArgs;

const DESPAWN_Z: f32 = -100000.0;

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) gid: vec3<u32>) {
    let idx = gid.x;
    let count = atomicLoad(&output_count.value);
    if (idx == 0u) {
        draw_args.instance_count = count;
    }
    if (idx >= count) {
        return;
    }

    var pos = positions[idx].xyz;
    var vel = velocities[idx].xyz;

    if (pos.z <= DESPAWN_Z) {
        return;
    }

    let region_max = settings.region_min + settings.region_size;
    if (pos.x < settings.region_min.x || pos.x > region_max.x || pos.y < settings.region_min.y || pos.y > region_max.y) {
        positions[idx] = vec4f(settings.region_min.x, settings.region_min.y, DESPAWN_Z, 1.0);
        velocities[idx] = vec4f(0.0, 0.0, 0.0, 0.0);
        return;
    }

    let uv = (pos.xy - settings.region_min) / settings.region_size;
    let uv_y = 1.0 - uv.y;

    let tex_size = textureDimensions(normal_texture);
    let tex_pos = vec2<i32>(clamp(vec2f(uv.x, uv_y) * vec2f(tex_size - 1), vec2f(0.0), vec2f(tex_size - 1)));

    let normal_raw = textureLoad(normal_texture, tex_pos, 0).xyz;
    let normal = normalize(normal_raw * 2.0 - 1.0);

    let gravity_vec = vec3f(0.0, 0.0, -settings.gravity);
    let accel = gravity_vec - dot(gravity_vec, normal) * normal;

    vel = vel + accel * settings.dt;
    vel = vel - dot(vel, normal) * normal;
    pos = pos + vel * settings.dt;

    if (pos.x < settings.region_min.x || pos.x > region_max.x || pos.y < settings.region_min.y || pos.y > region_max.y) {
        positions[idx] = vec4f(settings.region_min.x, settings.region_min.y, DESPAWN_Z, 1.0);
        velocities[idx] = vec4f(0.0, 0.0, 0.0, 0.0);
        return;
    }

    let uv_after = (pos.xy - settings.region_min) / settings.region_size;
    let uv_y_after = 1.0 - uv_after.y;
    let tex_pos_after = vec2<i32>(clamp(vec2f(uv_after.x, uv_y_after) * vec2f(tex_size - 1), vec2f(0.0), vec2f(tex_size - 1)));

    let height_value = textureLoad(height_texture, tex_pos_after, 0).x;
    let altitude_correction_factor = 1.0 / cos(y_to_lat(pos.y));
    pos.z = height_value * altitude_correction_factor;

    positions[idx] = vec4f(pos, 1.0);
    velocities[idx] = vec4f(vel, 0.0);
}
