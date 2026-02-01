#include "util/shared_config.wgsl"
#include "util/camera_config.wgsl"


@group(0) @binding(0) var<uniform> config: shared_config;
@group(1) @binding(0) var<uniform> camera: camera_config;
@group(2) @binding(0) var depth_texture: texture_2d<f32>;
@group(3) @binding(0) var<storage> positions: array<vec4f>;
@group(3) @binding(1) var<uniform> particle_config: ParticleConfig;
@group(3) @binding(2) var particle_texture: texture_2d<f32>;
@group(3) @binding(3) var particle_sampler: sampler;


struct ParticleConfig {
    color: vec4f,
    size: f32,
    _pad0: f32,
    _pad2: f32,
    _pad3: f32,
}

const quad_vertices = array<vec2f, 4>(
    vec2f(-0.5, -0.5),
    vec2f(0.5, -0.5),
    vec2f(-0.5, 0.5),
    vec2f(0.5, 0.5),
);

const quad_uvs = array<vec2f, 4>(
    vec2f(0.0, 1.0),
    vec2f(1.0, 1.0),
    vec2f(0.0, 0.0),
    vec2f(1.0, 0.0),
);

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
}

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index: u32, @builtin(instance_index) instance_index: u32) -> VertexOut {
    // Extract camera right and up vectors from view matrix to face the camera
    let right = normalize(vec3f(camera.view_matrix[0].xyz));
    let up = normalize(vec3f(camera.view_matrix[1].xyz));
    
    let half_size = particle_config.size * 0.5;
    let offset = (quad_vertices[vertex_index].x * right + quad_vertices[vertex_index].y * up) * half_size;
    
    var vertex_out: VertexOut;

    let pos = positions[instance_index];
    let world_position = pos.xyz + offset;
    vertex_out.position = camera.view_proj_matrix * vec4f(world_position - camera.position.xyz, 1);
    vertex_out.uv = quad_uvs[vertex_index];
    return vertex_out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    let texel = textureSample(particle_texture, particle_sampler, in.uv);
    if (texel.a < 0.1) {
        discard;
    }
    return vec4f(texel.rgb * particle_config.color.rgb, texel.a * particle_config.color.a);
}