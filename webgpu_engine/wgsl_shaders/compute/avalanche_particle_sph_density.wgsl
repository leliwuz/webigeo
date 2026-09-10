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

const INVALID_INDEX: u32 = 0xffffffffu;

fn position_to_cell_coords(position_xy: vec2f) -> vec2<i32> {
    let uv = (position_xy - settings.region_min) / settings.region_size;
    let grid_x = clamp(i32(uv.x * f32(settings.output_resolution.x)), 0, i32(settings.output_resolution.x) - 1);
    let grid_y = clamp(i32((1.0 - uv.y) * f32(settings.output_resolution.y)), 0, i32(settings.output_resolution.y) - 1);
    return vec2<i32>(grid_x, grid_y);
}

fn cell_coords_to_index(cell: vec2<i32>) -> u32 {
    return u32(cell.y) * settings.output_resolution.x + u32(cell.x);
}

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) gid: vec3<u32>) {
    let idx = gid.x;
    let count = atomicLoad(&output_count.value);
    if (idx >= count) {
        return;
    }

    let pos_i = positions[idx].xyz;
    var rho = 0.0;

    let base_cell = position_to_cell_coords(pos_i.xy);
    let grid_width = i32(settings.output_resolution.x);
    let grid_height = i32(settings.output_resolution.y);

    for (var offset_y: i32 = -1; offset_y <= 1; offset_y++) {
        for (var offset_x: i32 = -1; offset_x <= 1; offset_x++) {
            let neighbor_cell = base_cell + vec2<i32>(offset_x, offset_y);
            if (neighbor_cell.x < 0 || neighbor_cell.y < 0 || neighbor_cell.x >= grid_width || neighbor_cell.y >= grid_height) {
                continue;
            }

            var j = atomicLoad(&cell_heads[cell_coords_to_index(neighbor_cell)]);
            loop {
                if (j == INVALID_INDEX || j >= count) {
                    break;
                }

                let r = pos_i - positions[j].xyz;
                let r2 = dot(r, r);
                rho = rho + settings.sph_particle_mass * sph_poly6_kernel(r2, settings.sph_smoothing_length);
                j = particle_next[j];
            }
        }
    }

    rho = max(rho, settings.sph_rest_density * 0.5);
    densities[idx] = rho;
    pressures[idx] = max(0.0, settings.sph_pressure_stiffness * (rho - settings.sph_rest_density));
}
