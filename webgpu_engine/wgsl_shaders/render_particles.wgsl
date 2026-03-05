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
    // Get sphere vertex and normal
    let sphere_vertex = sphere_vertices[vertex_index].xyz;
    let sphere_normal = sphere_normals[vertex_index].xyz;
    
    // add particle position
    let particle_pos = positions[instance_index];
    let world_position = particle_pos.xyz + sphere_vertex;
    
    var vertex_out: VertexOut;
    vertex_out.position = camera.view_proj_matrix * vec4f(world_position - camera.position.xyz, 1);
    vertex_out.normal = sphere_normal;
    vertex_out.world_pos = world_position;
    return vertex_out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    // Simple lighting: use normal direction for shading
    let light_dir = normalize(vec3f(1.0, 1.0, 1.0));
    let normal = normalize(in.normal);
    let diffuse = max(dot(normal, light_dir), 0.2);
    
    return vec4f(particle_config.color.rgb * diffuse, particle_config.color.a);
}