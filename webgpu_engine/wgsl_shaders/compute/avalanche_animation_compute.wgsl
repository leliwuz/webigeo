struct AvalancheAnimationSettings {
    output_resolution: vec2<u32>,
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
@group(0) @binding(4) var normal_sampler: sampler;
@group(0) @binding(5) var height_sampler: sampler;
@group(0) @binding(6) var<storage, read_write> output_buffer: array<u32>;

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) gid: vec3<u32>) {
    let dims = textureDimensions(normal_texture);
    let width = dims.x;
    let height = dims.y;
    if (gid.x >= width || gid.y >= height) {
        return;
    }

    let idx = gid.y * width + gid.x;
    output_buffer[idx] = 0u;
}
