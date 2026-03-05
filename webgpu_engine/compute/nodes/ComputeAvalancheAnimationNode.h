#pragma once

#include "Node.h"
#include "webgpu_engine/Buffer.h"
#include "webgpu_engine/PipelineManager.h"


namespace webgpu_engine::compute::nodes {


class ComputeAvalancheAnimationNode : public Node {
    Q_OBJECT
public:
    static glm::uvec3 SHADER_WORKGROUP_SIZE; 
    
    struct AvalancheAnimationSettings {
        uint32_t resolution_multiplier = 1;
        uint32_t zoom_level = 18;
        WGPUTextureFormat texture_format = WGPUTextureFormat_RGBA8Unorm;
        WGPUTextureUsage texture_usage
            = (WGPUTextureUsage)(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc);
        float min_slope_angle = glm::radians(40.0f); // min slope angle [rad]
        float max_slope_angle = glm::radians(45.0f); // max slope angle [rad]
        glm::uvec2 sampling_interval = glm::uvec2(1, 1); // sampling interval in x and y direction [every sampling_interval texels]
        int num_particles_per_cell = 2; // number of particles to release per release cell
    };
private:
    struct AvalancheAnimationSettingsUniform {
        glm::uvec2 output_resolution;
        glm::fvec2 region_min;
        glm::fvec2 region_size;
        float min_slope_angle;
        float max_slope_angle;
        glm::uvec2 sampling_interval;
        uint32_t num_particles_per_cell;
        uint32_t padding_0;
    };

    struct AvalancheParticleStepSettingsUniform {
        glm::fvec2 region_min;
        glm::fvec2 region_size;
        glm::uvec2 output_resolution;
        float dt;
        float gravity;
    };

public:
    ComputeAvalancheAnimationNode(const PipelineManager& pipeline_manager, WGPUDevice device);
    ComputeAvalancheAnimationNode(const PipelineManager& pipeline_manager, WGPUDevice device, const AvalancheAnimationSettings& settings);

    void set_settings(const AvalancheAnimationSettings& settings);
    const AvalancheAnimationSettings& get_settings() const;

    void step_particles(float dt_seconds);

    public slots:
    void run_impl() override;
private:
    void update_gpu_settings(uint32_t run = 0u);

    std::unique_ptr<webgpu::raii::Sampler> create_normal_sampler(WGPUDevice device);
    std::unique_ptr<webgpu::raii::Sampler> create_height_sampler(WGPUDevice device);

private:
    const PipelineManager* m_pipeline_manager;
    WGPUDevice m_device;
    WGPUQueue m_queue;

    AvalancheAnimationSettings m_settings;
    webgpu_engine::Buffer<AvalancheAnimationSettingsUniform> m_settings_uniform;
    webgpu_engine::Buffer<AvalancheParticleStepSettingsUniform> m_step_settings_uniform;
    std::unique_ptr<webgpu::raii::Sampler> m_normal_sampler;
    std::unique_ptr<webgpu::raii::Sampler> m_height_sampler;
    
    std::unique_ptr<webgpu::raii::RawBuffer<glm::vec4>> m_output_storage_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_output_count_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<glm::vec4>> m_velocity_storage_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<uint32_t>> m_draw_indirect_args_buffer;

    const webgpu::raii::TextureWithSampler* m_cached_normal_texture = nullptr;
    const webgpu::raii::TextureWithSampler* m_cached_height_texture = nullptr;

    glm::uvec2 m_output_dimensions;
    //TODO: other Buffers / outputs for avalanche animation node when implemented (e.g. slope angle texture, etc.)
};
}