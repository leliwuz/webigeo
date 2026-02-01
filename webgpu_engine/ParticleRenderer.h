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
        glm::vec4 color = {255.f, 0.f, 0.f, 1.f};   
        float size = 50.0f;        
        float _pad0;      
        float _pad2;
        float _pad3;
    };

    struct ParticleState {
        Coordinates position;      // CPU position (in lat/long/alt)
        glm::vec3 velocity;      // For physics simulation
        glm::vec4 color;
        float size;
        float lifetime;           // Remaining lifetime (negative = infinite)
    };

public:
    ParticleRenderer(WGPUDevice device, const PipelineManager& pipeline_manager);
    
    // Spawn a new particle at the given position
    size_t spawn_particle(const Coordinates& coordinate, const glm::vec4& color = {255.f,0,0,1.f}, 
                          const glm::dvec3& velocity = {0.0, 0.0, -100.0}, float lifetime = -1.0f);
    
    // Update a specific particle's position
    void update_particle_position(size_t particle_id, const Coordinates& new_position);
    
    // Update all particles (physics simulation)
    void update_particles(float delta_time);

    void render(WGPUCommandEncoder command_encoder, 
                             const webgpu::raii::BindGroup& shared_config, 
                             const webgpu::raii::BindGroup& camera_config,
                             const webgpu::raii::BindGroup& depth_texture, 
                             const webgpu::raii::TextureView& color_texture);

    // Clear all particles
    void clear_particles() {
        m_particle_states.clear();
        m_position_buffers.clear();
        m_particle_config_buffers.clear();
        m_bind_groups.clear();
    }
    
    // Get number of active particles
    size_t particle_count() const { return m_particle_states.size(); }
    
    // Get particle state (read-only)
    const ParticleState* get_particle_state(size_t particle_id) const {
        return particle_id < m_particle_states.size() ? &m_particle_states[particle_id] : nullptr;
    }

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
    
    // Dummy texture and sampler for particles (required by bind group layout)
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_dummy_texture;
};

}
