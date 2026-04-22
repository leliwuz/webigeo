#include "util/tile_util.wgsl"
#include "util/sph_kernels.wgsl"

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
    padding_0: f32,
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

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) gid: vec3<u32>) {
    let idx = gid.x;
    let count = atomicLoad(&output_count.value);
    if (idx >= count) {
        return;
    }

    var pos_i = positions[idx].xyz;
    var vel_i = velocities[idx].xyz;
    if (pos_i.z <= DESPAWN_Z) {
        return;
    }

    let region_max = settings.region_min + settings.region_size;
    if (pos_i.x < settings.region_min.x || pos_i.x > region_max.x || pos_i.y < settings.region_min.y || pos_i.y > region_max.y) {
        positions[idx] = vec4f(settings.region_min.x, settings.region_min.y, DESPAWN_Z, 1.0);
        velocities[idx] = vec4f(0.0, 0.0, 0.0, 0.0);
        return;
    }

    let rho_i = max(densities[idx], settings.sph_epsilon);
    let p_i = pressures[idx];

    var pressure_force = vec3f(0.0);
    var viscosity_force = vec3f(0.0);

    for (var j: u32 = 0u; j < count; j++) {
        if (j == idx) {
            continue;
        }

        let pos_j = positions[j].xyz;
        let r = pos_i - pos_j;
        let r_len = length(r);
        if (r_len >= settings.sph_smoothing_length || r_len <= settings.sph_epsilon) {
            continue;
        }

        let rho_j = max(densities[j], settings.sph_epsilon);
        let p_j = pressures[j];
        let vel_j = velocities[j].xyz;

        pressure_force = pressure_force
            - settings.sph_particle_mass * ((p_i + p_j) / (2.0 * rho_j)) * sph_spiky_gradient(r, settings.sph_smoothing_length);

        let visc = sph_viscosity_laplacian(r_len, settings.sph_smoothing_length);
        viscosity_force = viscosity_force
            + settings.sph_viscosity * settings.sph_particle_mass * ((vel_j - vel_i) / rho_j) * visc;
    }

    let gravity_force = vec3f(0.0, 0.0, -settings.gravity * rho_i);
    let accel = (pressure_force + viscosity_force + gravity_force) / rho_i;

    vel_i = vel_i + accel * settings.dt;

    let speed = length(vel_i);
    if (speed > settings.sph_max_speed) {
        let scale = settings.sph_max_speed / max(speed, settings.sph_epsilon);
        vel_i = vel_i * scale;
    }

    pos_i = pos_i + vel_i * settings.dt;

    if (pos_i.x < settings.region_min.x || pos_i.x > region_max.x || pos_i.y < settings.region_min.y || pos_i.y > region_max.y) {
        positions[idx] = vec4f(settings.region_min.x, settings.region_min.y, DESPAWN_Z, 1.0);
        velocities[idx] = vec4f(0.0, 0.0, 0.0, 0.0);
        return;
    }

    let uv = (pos_i.xy - settings.region_min) / settings.region_size;
    let uv_y = 1.0 - uv.y;

    let tex_size = textureDimensions(normal_texture);
    let tex_pos = vec2<i32>(clamp(vec2f(uv.x, uv_y) * vec2f(tex_size - 1), vec2f(0.0), vec2f(tex_size - 1)));

    let normal_raw = textureLoad(normal_texture, tex_pos, 0).xyz;
    let normal = normalize(normal_raw * 2.0 - 1.0);
    vel_i = vel_i - dot(vel_i, normal) * normal;

    let height_value = textureLoad(height_texture, tex_pos, 0).x;
    let altitude_correction_factor = 1.0 / cos(y_to_lat(pos_i.y));
    pos_i.z = height_value * altitude_correction_factor;

    positions[idx] = vec4f(pos_i, 1.0);
    velocities[idx] = vec4f(vel_i, 0.0);
}
