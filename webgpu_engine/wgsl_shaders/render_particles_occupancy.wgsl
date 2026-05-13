#include "util/shared_config.wgsl"
#include "util/camera_config.wgsl"

@group(0) @binding(0) var<uniform> config: shared_config;
@group(1) @binding(0) var<uniform> camera: camera_config;
@group(2) @binding(0) var depth_texture: texture_2d<f32>;
@group(3) @binding(0) var<storage> positions: array<vec4f>;
@group(3) @binding(1) var<uniform> particle_config: ParticleConfig;
@group(3) @binding(2) var<storage> sphere_vertices: array<vec4f>;
@group(3) @binding(3) var<storage> sphere_normals: array<vec4f>;

struct ParticleConfig {
    color: vec4f,
    _pad4: f32,
    _pad0: f32,
    _pad2: f32,
    _pad3: f32,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
    @location(1) world_pos: vec3f,
}

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index: u32, @builtin(instance_index) instance_index: u32) -> VertexOut {
    var vertex_out: VertexOut;
    vertex_out.position = vec4f(0.0, 0.0, 0.0, 1.0);
    vertex_out.normal = vec3f(0.0, 0.0, 1.0);
    vertex_out.world_pos = vec3f(0.0, 0.0, 0.0);
    return vertex_out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    return vec4f(0.0, 0.0, 0.0, 0.0);
}
