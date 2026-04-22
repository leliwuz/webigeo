#include "util/tile_util.wgsl"
#include "util/normals_util.wgsl"

struct ParticleStepSettings {
    region_min: vec2f,
    region_size: vec2f,
    output_resolution: vec2u,
    dt: f32,
    gravity: f32,
    sph_smoothing_length: f32,
    sph_particle_mass: f32,
    sph_rest_density: f32,
    sph_pressure_stiffness: f32,
    sph_viscosity: f32,
    sph_epsilon: f32,
    sph_max_speed: f32,
    sflm_friction_angle: f32,
    sflm_min_travel_angle: f32,
    sflm_max_velocity: f32,
    sflm_damping: f32,
    sflm_stop_velocity: f32,
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
@group(0) @binding(7) var<storage, read_write> densities: array<f32>;
@group(0) @binding(8) var<storage, read_write> pressures: array<f32>;

const DESPAWN_Z: f32 = -100000.0;

fn safe_dir2(primary: vec2f, secondary: vec2f, eps: f32) -> vec2f {
    let p_len = length(primary);
    if (p_len > eps) {
        return primary / p_len;
    }

    let s_len = length(secondary);
    if (s_len > eps) {
        return secondary / s_len;
    }

    return vec2f(1.0, 0.0);
}

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) gid: vec3<u32>) {
    let idx = gid.x;
    let count = atomicLoad(&output_count.value);
    if (idx >= count) {
        return;
    }

    let pos = positions[idx].xyz;
    if (pos.z <= DESPAWN_Z) {
        return;
    }

    var vel = velocities[idx].xyz;
    let v0 = length(vel);

    if (v0 <= settings.sflm_stop_velocity) {
        velocities[idx] = vec4f(0.0, 0.0, 0.0, 0.0);
        return;
    }

    let uv = (pos.xy - settings.region_min) / settings.region_size;
    let uv_y = 1.0 - uv.y;
    if (uv.x < 0.0 || uv.x > 1.0 || uv_y < 0.0 || uv_y > 1.0) {
        return;
    }

    let tex_size = textureDimensions(height_texture);
    let tex_pos = vec2<i32>(clamp(vec2f(uv.x, uv_y) * vec2f(tex_size - 1), vec2f(0.0), vec2f(tex_size - 1)));

    let height_here = textureLoad(height_texture, tex_pos, 0).x;
    let normal_raw = textureLoad(normal_texture, tex_pos, 0).xyz;
    let normal = normalize(normal_raw * 2.0 - 1.0);

    let downhill2 = vec2f(-normal.x, -normal.y);
    let dir2 = safe_dir2(vel.xy, downhill2, settings.sph_epsilon);

    let dX = max(length(vel.xy) * settings.dt, settings.sph_epsilon);
    let region_safe = max(settings.region_size, vec2f(settings.sph_epsilon));
    let uv_step = dir2 * (dX / region_safe);
    let uv_next = clamp(uv + uv_step, vec2f(0.0), vec2f(1.0));
    let uv_next_y = 1.0 - uv_next.y;
    let tex_pos_next = vec2<i32>(clamp(vec2f(uv_next.x, uv_next_y) * vec2f(tex_size - 1), vec2f(0.0), vec2f(tex_size - 1)));

    let height_next = textureLoad(height_texture, tex_pos_next, 0).x;
    let dH = height_here - height_next;

    let phi = settings.sflm_friction_angle;
    let energy_term = v0 * v0 + 2.0 * settings.gravity * dH - 2.0 * settings.gravity * dX * tan(phi);

    let travel_angle = atan2(max(dH, 0.0), max(dX, settings.sph_epsilon));
    if (travel_angle < settings.sflm_min_travel_angle) {
        vel = vel * settings.sflm_damping;
    }

    if (energy_term <= 0.0) {
        vel = vel * settings.sflm_damping;
        if (length(vel) < settings.sflm_stop_velocity) {
            vel = vec3f(0.0);
        }
        velocities[idx] = vec4f(vel, 0.0);
        return;
    }

    let target_speed = min(settings.sflm_max_velocity, sqrt(energy_term));
    let current_speed = max(length(vel), settings.sph_epsilon);
    vel = vel * (target_speed / current_speed);
    if (target_speed < settings.sflm_stop_velocity) {
        vel = vec3f(0.0);
    }

    velocities[idx] = vec4f(vel, 0.0);
}