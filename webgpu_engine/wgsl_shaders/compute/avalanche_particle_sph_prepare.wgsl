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
@group(0) @binding(10) var<storage, read_write> cell_heads: array<atomic<u32>>;
@group(0) @binding(11) var<storage, read_write> particle_next: array<u32>;

const DESPAWN_Z: f32 = -100000.0;
const INVALID_INDEX: u32 = 0xffffffffu;

fn position_to_cell_index(position_xy: vec2f) -> u32 {
    let uv = (position_xy - settings.region_min) / settings.region_size;
    let grid_x = clamp(u32(uv.x * f32(settings.output_resolution.x)), 0u, settings.output_resolution.x - 1u);
    let grid_y = clamp(u32((1.0 - uv.y) * f32(settings.output_resolution.y)), 0u, settings.output_resolution.y - 1u);
    return grid_y * settings.output_resolution.x + grid_x;
}

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) gid: vec3<u32>) {
    let idx = gid.x;
    let total_cells = settings.output_resolution.x * settings.output_resolution.y;

    if (idx < total_cells) {
        atomicStore(&cell_heads[idx], INVALID_INDEX);
    }

    let count = atomicLoad(&output_count.value);

    if (idx == 0u) {
        draw_args.instance_count = count;
    }

    if (idx >= count) {
        return;
    }

    densities[idx] = 0.0;
    pressures[idx] = 0.0;

    let pos = positions[idx].xyz;
    particle_next[idx] = INVALID_INDEX;
    if (pos.z <= DESPAWN_Z) {
        return;
    }

    let region_max = settings.region_min + settings.region_size;
    if (pos.x < settings.region_min.x || pos.x > region_max.x || pos.y < settings.region_min.y || pos.y > region_max.y) {
        return;
    }

    let cell_idx = position_to_cell_index(pos.xy);
    let previous_head = atomicExchange(&cell_heads[cell_idx], idx);
    particle_next[idx] = previous_head;
}
