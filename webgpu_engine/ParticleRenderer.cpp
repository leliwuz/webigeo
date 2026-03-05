
#include "ParticleRenderer.h"

#include <webgpu/webgpu.h>
#include "nucleus/srs.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace webgpu_engine{

ParticleRenderer::ParticleRenderer(WGPUDevice device, const PipelineManager& pipeline_manager)
    : m_device { device }
    , m_queue { wgpuDeviceGetQueue(device) }
    , m_pipeline_manager { &pipeline_manager }
{
    // Generate sphere mesh (shared for all particles)
    generate_sphere_mesh(16, 8, 2.0f);
    
    spawn_particle(Coordinates{46.6,13.7,2000.6}, glm::vec4{1.f,1.f,1.f,1.f});
}


size_t ParticleRenderer::spawn_particle(const Coordinates& coordinate, const glm::vec4& color, 
                                         const glm::dvec3& velocity)
{
    // Store CPU-side particle state
    ParticleState state;
    state.position = coordinate;
    state.velocity = velocity;
    state.color = color;
    state.is_world_space = false;
    m_particle_states.push_back(state);
    
    // Convert coordinate to world position
    glm::fvec4 world_position(nucleus::srs::lat_long_alt_to_world(coordinate), 1.0f);

    // Create position buffer for this particle
    m_position_buffers.emplace_back(std::make_unique<webgpu::raii::RawBuffer<glm::fvec4>>(
        m_device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, 1, "particle renderer, position buffer"));
    m_position_buffers.back()->write(m_queue, &world_position, 1);

    // Create particle config buffer
    auto config_buffer = std::make_unique<webgpu_engine::Buffer<ParticleConfig>>(
        m_device, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
    config_buffer->data.color = color;
    config_buffer->update_gpu_data(m_queue);

    m_particle_config_buffers.emplace_back(std::move(config_buffer));

    // Create bind group for this particle (positions, config, sphere vertices, sphere normals)
    m_bind_groups.emplace_back(std::make_unique<webgpu::raii::BindGroup>(
        m_device, m_pipeline_manager->particles_bind_group_layout(),
        std::initializer_list<WGPUBindGroupEntry> {
            m_position_buffers.back()->create_bind_group_entry(0),
            m_particle_config_buffers.back()->raw_buffer().create_bind_group_entry(1),
            m_sphere_vertices->create_bind_group_entry(2),
            m_sphere_normals->create_bind_group_entry(3)
        }));
    
    return m_particle_states.size() - 1;  // Return particle ID
}

size_t ParticleRenderer::spawn_particle_world(const glm::dvec3& world_position, const glm::vec4& color,
                                              const glm::dvec3& velocity)
{
    ParticleState state;
    state.position = world_position;
    state.velocity = velocity;
    state.color = color;
    state.is_world_space = true;
    m_particle_states.push_back(state);

    glm::fvec4 gpu_position(world_position, 1.0f);

    m_position_buffers.emplace_back(std::make_unique<webgpu::raii::RawBuffer<glm::fvec4>>(
        m_device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, 1, "particle renderer, position buffer"));
    m_position_buffers.back()->write(m_queue, &gpu_position, 1);

    auto config_buffer = std::make_unique<webgpu_engine::Buffer<ParticleConfig>>(
        m_device, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
    config_buffer->data.color = color;
    config_buffer->update_gpu_data(m_queue);

    m_particle_config_buffers.emplace_back(std::move(config_buffer));

    m_bind_groups.emplace_back(std::make_unique<webgpu::raii::BindGroup>(
        m_device, m_pipeline_manager->particles_bind_group_layout(),
        std::initializer_list<WGPUBindGroupEntry> {
            m_position_buffers.back()->create_bind_group_entry(0),
            m_particle_config_buffers.back()->raw_buffer().create_bind_group_entry(1),
            m_sphere_vertices->create_bind_group_entry(2),
            m_sphere_normals->create_bind_group_entry(3)
        }));

    return m_particle_states.size() - 1;
}

void ParticleRenderer::update_particle_position(size_t particle_id, const Coordinates& new_position)
{
    if (particle_id >= m_particle_states.size()) return;
    
    // Update CPU state
    m_particle_states[particle_id].position = new_position;
    
    // Update GPU buffer
    if (m_particle_states[particle_id].is_world_space) {
        glm::fvec4 world_position(new_position, 1.0f);
        m_position_buffers[particle_id]->write(m_queue, &world_position, 1);
    } else {
        glm::fvec4 world_position(nucleus::srs::lat_long_alt_to_world(new_position), 1.0f);
        m_position_buffers[particle_id]->write(m_queue, &world_position, 1);
    }
}

void ParticleRenderer::update_particles(float delta_time)
{
    for (size_t i = 0; i < m_particle_states.size(); ++i) {
        auto& state = m_particle_states[i];
        
        // Apply velocity to position
        state.position += state.velocity * delta_time;
        
        // Update GPU buffer
        if (state.is_world_space) {
            glm::fvec4 world_position(state.position, 1.0f);
            m_position_buffers[i]->write(m_queue, &world_position, 1);
        } else {
            glm::fvec4 world_position(nucleus::srs::lat_long_alt_to_world(state.position), 1.0f);
            m_position_buffers[i]->write(m_queue, &world_position, 1);
        }
    }
}

void ParticleRenderer::render(WGPUCommandEncoder command_encoder, 
                             const webgpu::raii::BindGroup& shared_config, 
                             const webgpu::raii::BindGroup& camera_config,
                             const webgpu::raii::BindGroup& depth_texture, 
                             const webgpu::raii::TextureView& color_texture)
{
    // Early return if no particles to render
    if (m_bind_groups.empty()) {
        return;
    }

    // Setup render pass for particles
    WGPURenderPassColorAttachment color_attachment {};
    color_attachment.view = color_texture.handle();
    color_attachment.resolveTarget = nullptr;
    color_attachment.loadOp = WGPULoadOp::WGPULoadOp_Load;  // Load existing content
    color_attachment.storeOp = WGPUStoreOp::WGPUStoreOp_Store;
    color_attachment.clearValue = WGPUColor { 0.0, 0.0, 0.0, 0.0 };
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    WGPURenderPassDescriptor render_pass_descriptor {};
    render_pass_descriptor.label = WGPUStringView { .data = "particle render pass", .length = WGPU_STRLEN };
    render_pass_descriptor.colorAttachmentCount = 1;
    render_pass_descriptor.colorAttachments = &color_attachment;
    render_pass_descriptor.depthStencilAttachment = nullptr;
    render_pass_descriptor.timestampWrites = nullptr;

    auto render_pass = webgpu::raii::RenderPassEncoder(command_encoder, render_pass_descriptor);
    
    // Set pipeline
    wgpuRenderPassEncoderSetPipeline(render_pass.handle(), m_pipeline_manager->render_particles_pipeline().handle());
    
    // Bind common groups (group 0: shared, group 1: camera, group 2: depth)
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 0, shared_config.handle(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 1, camera_config.handle(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 2, depth_texture.handle(), 0, nullptr);
    
    // Draw each particle (using shared sphere mesh)
    for (size_t i = 0; i < m_bind_groups.size(); i++) {
        // Bind particle-specific group (group 3: positions, config, sphere vertices, sphere normals)
        wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 3, m_bind_groups.at(i)->handle(), 0, nullptr);
        
        // Draw sphere vertices with instancing
        wgpuRenderPassEncoderDraw(render_pass.handle(), m_sphere_index_count, 1, 0, 0);
    }
}

void ParticleRenderer::setup_gpu_driven(const webgpu::raii::RawBuffer<glm::vec4>* positions,
    const webgpu::raii::RawBuffer<uint32_t>* indirect_args,
    const glm::vec4& color)
{
    if (!positions || !indirect_args) {
        clear_gpu_driven();
        return;
    }

    const bool needs_bind_group = !m_gpu_bind_group || m_gpu_positions_buffer != positions;
    m_gpu_positions_buffer = positions;
    m_gpu_indirect_args_buffer = indirect_args;

    const bool created_config = (m_gpu_particle_config_buffer == nullptr);
    if (!m_gpu_particle_config_buffer) {
        m_gpu_particle_config_buffer = std::make_unique<webgpu_engine::Buffer<ParticleConfig>>(
            m_device, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
    }
    if (created_config || color.x != m_gpu_particle_color.x || color.y != m_gpu_particle_color.y
        || color.z != m_gpu_particle_color.z || color.w != m_gpu_particle_color.w) {
        m_gpu_particle_color = color;
        m_gpu_particle_config_buffer->data.color = color;
        m_gpu_particle_config_buffer->update_gpu_data(m_queue);
    }

    if (needs_bind_group) {
        m_gpu_bind_group = std::make_unique<webgpu::raii::BindGroup>(
            m_device, m_pipeline_manager->particles_bind_group_layout(),
            std::initializer_list<WGPUBindGroupEntry> {
                m_gpu_positions_buffer->create_bind_group_entry(0),
                m_gpu_particle_config_buffer->raw_buffer().create_bind_group_entry(1),
                m_sphere_vertices->create_bind_group_entry(2),
                m_sphere_normals->create_bind_group_entry(3)
            });
    }
}

void ParticleRenderer::render_gpu_driven(WGPUCommandEncoder command_encoder,
    const webgpu::raii::BindGroup& shared_config,
    const webgpu::raii::BindGroup& camera_config,
    const webgpu::raii::BindGroup& depth_texture,
    const webgpu::raii::TextureView& color_texture)
{
    if (!m_gpu_bind_group || !m_gpu_indirect_args_buffer) {
        return;
    }

    WGPURenderPassColorAttachment color_attachment {};
    color_attachment.view = color_texture.handle();
    color_attachment.resolveTarget = nullptr;
    color_attachment.loadOp = WGPULoadOp::WGPULoadOp_Load;
    color_attachment.storeOp = WGPUStoreOp::WGPUStoreOp_Store;
    color_attachment.clearValue = WGPUColor { 0.0, 0.0, 0.0, 0.0 };
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    WGPURenderPassDescriptor render_pass_descriptor {};
    render_pass_descriptor.label = WGPUStringView { .data = "particle render pass (gpu-driven)", .length = WGPU_STRLEN };
    render_pass_descriptor.colorAttachmentCount = 1;
    render_pass_descriptor.colorAttachments = &color_attachment;
    render_pass_descriptor.depthStencilAttachment = nullptr;
    render_pass_descriptor.timestampWrites = nullptr;

    auto render_pass = webgpu::raii::RenderPassEncoder(command_encoder, render_pass_descriptor);

    wgpuRenderPassEncoderSetPipeline(render_pass.handle(), m_pipeline_manager->render_particles_pipeline().handle());
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 0, shared_config.handle(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 1, camera_config.handle(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 2, depth_texture.handle(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 3, m_gpu_bind_group->handle(), 0, nullptr);

    wgpuRenderPassEncoderDrawIndirect(render_pass.handle(), m_gpu_indirect_args_buffer->handle(), 0);
}

void ParticleRenderer::clear_gpu_driven()
{
    m_gpu_positions_buffer = nullptr;
    m_gpu_indirect_args_buffer = nullptr;
    m_gpu_bind_group.reset();
    m_gpu_particle_config_buffer.reset();
}

void ParticleRenderer::generate_sphere_mesh(uint32_t longitude_segments, uint32_t latitude_segments, float radius)
{
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uv; // Not used yet

    positions.push_back({0.0f, radius, 0.0f});
    positions.push_back({0.0f, -radius, 0.0f});
    normals.push_back({0.0f, 1.0f, 0.0f});
    normals.push_back({0.0f, -1.0f, 0.0f});
    uv.push_back(glm::vec2(0.f,0.f));
    uv.push_back(glm::vec2(0.f,1.f));

    for (uint32_t j = 0; j < longitude_segments; j++) {
        indices.push_back(0);
        indices.push_back(j == longitude_segments - 1 ? 2 : (j + 3));
        indices.push_back(2 + j);

        indices.push_back(2 + (latitude_segments - 2) * longitude_segments + j);
        indices.push_back(
            j == longitude_segments - 1 ? 2 + (latitude_segments - 2) * longitude_segments
                                        : 2 + (latitude_segments - 2) * longitude_segments + j + 1
        );
        indices.push_back(1);
    }
    
    // vertices and rings
    for (uint32_t i = 1; i < latitude_segments; i++) {
        float verticalAngle = float(i) * glm::pi<float>() / float(latitude_segments);
        for (uint32_t j = 0; j < longitude_segments; j++) {
            float horizontalAngle = float(j) * 2.0f * glm::pi<float>() / float(longitude_segments);
            glm::vec3 position = glm::vec3(
                radius * glm::sin(verticalAngle) * glm::cos(horizontalAngle),
                radius * glm::cos(verticalAngle),
                radius * glm::sin(verticalAngle) * glm::sin(horizontalAngle)
            );
            positions.push_back(position);

            uv.push_back(glm::vec2(1.0f - float(j) / float(longitude_segments), float(i) / float(latitude_segments)));

            normals.push_back(glm::normalize(position));

            if (i == 1)
                continue;

            indices.push_back(2 + (i - 1) * longitude_segments + j);
            indices.push_back(j == longitude_segments - 1 ? 2 + (i - 2) * longitude_segments : 2 + (i - 2) * longitude_segments + j + 1);
            indices.push_back(j == longitude_segments - 1 ? 2 + (i - 1) * longitude_segments : 2 + (i - 1) * longitude_segments + j + 1);

            indices.push_back(j == longitude_segments - 1 ? 2 + (i - 2) * longitude_segments : 2 + (i - 2) * longitude_segments + j + 1);
            indices.push_back(2 + (i - 1) * longitude_segments + j);
            indices.push_back(2 + (i - 2) * longitude_segments + j);
        }
    }
    
    // Expand the indexed mesh into a non-indexed mesh (de-indexing)
    // Use vec4 to match storage buffer alignment (vec3 has 16-byte alignment in WGSL)
    std::vector<glm::vec4> expanded_positions;
    std::vector<glm::vec4> expanded_normals;
    expanded_positions.reserve(indices.size());
    expanded_normals.reserve(indices.size());
    
    for (uint32_t index : indices) {
        expanded_positions.push_back(glm::vec4(positions[index], 1.0f));
        expanded_normals.push_back(glm::vec4(normals[index], 0.0f));
    }
    
    m_sphere_index_count = static_cast<uint32_t>(expanded_positions.size());
    
    // Create GPU buffers for sphere mesh (using expanded vertices)
    m_sphere_vertices = std::make_unique<webgpu::raii::RawBuffer<glm::fvec4>>(
        m_device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, 
        expanded_positions.size(), "particle sphere vertices");
    m_sphere_vertices->write(m_queue, expanded_positions.data(), expanded_positions.size());
    
    m_sphere_normals = std::make_unique<webgpu::raii::RawBuffer<glm::fvec4>>(
        m_device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, 
        expanded_normals.size(), "particle sphere normals");
    m_sphere_normals->write(m_queue, expanded_normals.data(), expanded_normals.size());
}

}
