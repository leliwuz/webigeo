#pragma once

#include "Buffer.h"
#include "PipelineManager.h"
#include "webgpu/raii/BindGroup.h"
#include "webgpu/raii/RawBuffer.h"
#include "webgpu/raii/TextureWithSampler.h"
#include <glm/glm.hpp>
#include <vector>

namespace webgpu_engine{

using Coordinates = glm::dvec3;


class ParticleRenderer{
public: 
    struct ParticleConfig {
        glm::vec4 color = {1.f, 0.f, 0.f, 1.f};   
        float _pad4;        
        float _pad0;      
        float _pad2;
        float _pad3;
    };

    struct ParticleState {
        Coordinates position;      // CPU position (lat/long/alt or world-space)
        glm::vec3 velocity;      // For physics simulation
        glm::vec4 color;
        float pad0;
        bool is_world_space = false;
    };

public:
    ParticleRenderer(WGPUDevice device, const PipelineManager& pipeline_manager);
    
    // Spawn a new particle at the given position
    size_t spawn_particle(const Coordinates& coordinate, const glm::vec4& color = {1.f,1.0,1.0,1.f}, 
                          const glm::dvec3& velocity = {0.0, 0.0, 0.0});

    // Spawn a new particle using world-space coordinates directly
    size_t spawn_particle_world(const glm::dvec3& world_position, const glm::vec4& color = {1.f,1.0,1.0,1.f},
                                const glm::dvec3& velocity = {0.0, 0.0, 0.0});
    
    // Update a specific particle's position
    void update_particle_position(size_t particle_id, const Coordinates& new_position);
    
    // Update all particles (physics simulation)
    void update_particles(float delta_time);

    void render(WGPUCommandEncoder command_encoder, 
                             const webgpu::raii::BindGroup& shared_config, 
                             const webgpu::raii::BindGroup& camera_config,
                             const webgpu::raii::BindGroup& depth_texture, 
                             const webgpu::raii::TextureView& color_texture);

    void setup_gpu_driven(const webgpu::raii::RawBuffer<glm::vec4>* positions,
        const webgpu::raii::RawBuffer<uint32_t>* indirect_args,
        const glm::vec4& color = { 1.f, 1.0, 1.0, 1.f });

    void render_gpu_driven(WGPUCommandEncoder command_encoder,
        const webgpu::raii::BindGroup& shared_config,
        const webgpu::raii::BindGroup& camera_config,
        const webgpu::raii::BindGroup& depth_texture,
        const webgpu::raii::TextureView& color_texture);

    void clear_gpu_driven();

    // Clear all particles
    void clear_particles() {
        m_particle_states.clear();
        m_position_buffers.clear();
        m_particle_config_buffers.clear();
        m_bind_groups.clear();
        clear_gpu_driven();
    }
    
    // Get number of active particles
    size_t particle_count() const { return m_particle_states.size(); }
    
    // Get particle state (read-only)
    const ParticleState* get_particle_state(size_t particle_id) const {
        return particle_id < m_particle_states.size() ? &m_particle_states[particle_id] : nullptr;
    }

    uint32_t sphere_vertex_count() const { return m_sphere_index_count; }

private:
    WGPUDevice m_device;
    WGPUQueue m_queue;
    const PipelineManager* m_pipeline_manager;

    // CPU-side particle state for updates and physics
    std::vector<ParticleState> m_particle_states;
    
    // GPU-side buffers (one per particle)
    std::vector<std::unique_ptr<webgpu::raii::RawBuffer<glm::fvec4>>> m_position_buffers;
    std::vector<std::unique_ptr<webgpu_engine::Buffer<ParticleRenderer::ParticleConfig>>> m_particle_config_buffers;
    std::vector<std::unique_ptr<webgpu::raii::BindGroup>> m_bind_groups;
    
    // Shared sphere mesh data for all particles
    std::unique_ptr<webgpu::raii::RawBuffer<glm::fvec4>> m_sphere_vertices;
    std::unique_ptr<webgpu::raii::RawBuffer<glm::fvec4>> m_sphere_normals;
    uint32_t m_sphere_index_count = 0;  // Actually vertex count for non-indexed drawing

    // GPU-driven particle rendering state
    const webgpu::raii::RawBuffer<glm::vec4>* m_gpu_positions_buffer = nullptr;
    const webgpu::raii::RawBuffer<uint32_t>* m_gpu_indirect_args_buffer = nullptr;
    std::unique_ptr<webgpu_engine::Buffer<ParticleRenderer::ParticleConfig>> m_gpu_particle_config_buffer;
    std::unique_ptr<webgpu::raii::BindGroup> m_gpu_bind_group;
    glm::vec4 m_gpu_particle_color = { 1.f, 1.0, 1.0, 1.f };
    
    // Helper method to generate sphere mesh
    void generate_sphere_mesh(uint32_t longitude_segments = 32, uint32_t latitude_segments = 16, float radius = 1.0f);
};

}
