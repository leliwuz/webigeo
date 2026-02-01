
#include "ParticleRenderer.h"

#include <webgpu/webgpu.h>
#include "nucleus/srs.h"

namespace webgpu_engine{

ParticleRenderer::ParticleRenderer(WGPUDevice device, const PipelineManager& pipeline_manager)
    : m_device { device }
    , m_queue { wgpuDeviceGetQueue(device) }
    , m_pipeline_manager { &pipeline_manager }
{
    // Create a 1x1 dummy texture for the bind group layout
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "particle dummy texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { 1, 1, 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = WGPUTextureFormat::WGPUTextureFormat_RGBA8Unorm;
    texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "particle dummy sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Nearest;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;

    m_dummy_texture = std::make_unique<webgpu::raii::TextureWithSampler>(m_device, texture_desc, sampler_desc);
    
    // Write a white pixel to the dummy texture so particles are visible
    uint8_t white_pixel[4] = { 255, 0, 0, 255 };
    WGPUTexelCopyTextureInfo destination {};
    destination.texture = m_dummy_texture->texture().handle();
    destination.mipLevel = 0;
    destination.origin = { 0, 0, 0 };
    destination.aspect = WGPUTextureAspect_All;
    
    WGPUTexelCopyBufferLayout data_layout {};
    data_layout.offset = 0;
    data_layout.bytesPerRow = 4;
    data_layout.rowsPerImage = 1;
    
    WGPUExtent3D write_size { 1, 1, 1 };
    wgpuQueueWriteTexture(m_queue, &destination, white_pixel, 4, &data_layout, &write_size);
    
    wgpuQueueSubmit(m_queue, 0, nullptr);

    spawn_particle(Coordinates{46.6,13.7,2000.6}, glm::vec4{255.f,0,0,1.f});
}


size_t ParticleRenderer::spawn_particle(const Coordinates& coordinate, const glm::vec4& color, 
                                         const glm::dvec3& velocity, float lifetime)
{
    // Store CPU-side particle state
    ParticleState state;
    state.position = coordinate;
    state.velocity = velocity;
    state.color = color;
    state.size = 50.0f;
    state.lifetime = lifetime;
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
    config_buffer->data.size = 50.0f;
    config_buffer->update_gpu_data(m_queue);

    m_particle_config_buffers.emplace_back(std::move(config_buffer));

    // Create bind group for this particle (positions, config, texture, sampler)
    m_bind_groups.emplace_back(std::make_unique<webgpu::raii::BindGroup>(
        m_device, m_pipeline_manager->particles_bind_group_layout(),
        std::initializer_list<WGPUBindGroupEntry> {
            m_position_buffers.back()->create_bind_group_entry(0),
            m_particle_config_buffers.back()->raw_buffer().create_bind_group_entry(1),
            m_dummy_texture->texture_view().create_bind_group_entry(2),
            m_dummy_texture->sampler().create_bind_group_entry(3)
        }));
    
    return m_particle_states.size() - 1;  // Return particle ID
}

void ParticleRenderer::update_particle_position(size_t particle_id, const Coordinates& new_position)
{
    if (particle_id >= m_particle_states.size()) return;
    
    // Update CPU state
    m_particle_states[particle_id].position = new_position;
    
    // Update GPU buffer
    glm::fvec4 world_position(nucleus::srs::lat_long_alt_to_world(new_position), 1.0f);
    m_position_buffers[particle_id]->write(m_queue, &world_position, 1);
}

void ParticleRenderer::update_particles(float delta_time)
{
    for (size_t i = 0; i < m_particle_states.size(); ++i) {
        auto& state = m_particle_states[i];
        
        // Update lifetime
        if (state.lifetime > 0.0f) {
            state.lifetime -= delta_time;
            if (state.lifetime <= 0.0f) {
                // Particle expired - could remove it here
                continue;
            }
        }
        
        // Apply velocity to position
        state.position += state.velocity * delta_time;
        
        // Update GPU buffer
        glm::fvec4 world_position(nucleus::srs::lat_long_alt_to_world(state.position), 1.0f);
        m_position_buffers[i]->write(m_queue, &world_position, 1);
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
    
    // Draw each particle
    for (size_t i = 0; i < m_bind_groups.size(); i++) {
        // Bind particle-specific group (group 3: positions, config, texture, sampler)
        wgpuRenderPassEncoderSetBindGroup(render_pass.handle(), 3, m_bind_groups.at(i)->handle(), 0, nullptr);
        
        // Draw 4 vertices (triangle strip for quad), 1 instance (this particle)
        wgpuRenderPassEncoderDraw(render_pass.handle(), 4, 1, 0, 0);
    }
}

}
