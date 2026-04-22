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

@compute @workgroup_size(1, 1, 1)
fn computeMain(@builtin(global_invocation_id) gid: vec3<u32>) {
    if (gid.x != 0u) {
        return;
    }

    let count = atomicLoad(&output_count.value);
    var write_idx: u32 = 0u;

    for (var read_idx: u32 = 0u; read_idx < count; read_idx++) {
        let pos = positions[read_idx];
        if (pos.z <= DESPAWN_Z) {
            continue;
        }

        if (write_idx != read_idx) {
            positions[write_idx] = pos;
            velocities[write_idx] = velocities[read_idx];
            densities[write_idx] = densities[read_idx];
            pressures[write_idx] = pressures[read_idx];
        }

        write_idx++;
    }

    for (var clear_idx: u32 = write_idx; clear_idx < count; clear_idx++) {
        positions[clear_idx] = vec4f(settings.region_min.x, settings.region_min.y, DESPAWN_Z, 1.0);
        velocities[clear_idx] = vec4f(0.0, 0.0, 0.0, 0.0);
        densities[clear_idx] = 0.0;
        pressures[clear_idx] = 0.0;
    }

    atomicStore(&output_count.value, write_idx);
    draw_args.instance_count = write_idx;
}
