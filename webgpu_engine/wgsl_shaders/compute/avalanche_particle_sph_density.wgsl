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

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) gid: vec3<u32>) {
    let idx = gid.x;
    let count = atomicLoad(&output_count.value);
    if (idx >= count) {
        return;
    }

    let pos_i = positions[idx].xyz;
    var rho = 0.0;

    for (var j: u32 = 0u; j < count; j++) {
        let r = pos_i - positions[j].xyz;
        let r2 = dot(r, r);
        rho = rho + settings.sph_particle_mass * sph_poly6_kernel(r2, settings.sph_smoothing_length);
    }

    rho = max(rho, settings.sph_rest_density * 0.5);
    densities[idx] = rho;
    pressures[idx] = max(0.0, settings.sph_pressure_stiffness * (rho - settings.sph_rest_density));
}
