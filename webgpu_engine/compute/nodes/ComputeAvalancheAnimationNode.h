#pragma once

#include "Node.h"
#include "webgpu_engine/Buffer.h"
#include "webgpu_engine/PipelineManager.h"

#include <type_traits>


namespace webgpu_engine::compute::nodes {


class ComputeAvalancheAnimationNode : public Node {
    Q_OBJECT
public:
    static glm::uvec3 SHADER_WORKGROUP_SIZE; 
    
    struct AvalancheAnimationSettings {
        uint32_t resolution_multiplier = 1;
        uint32_t zoom_level = 16;
        WGPUTextureFormat texture_format = WGPUTextureFormat_RGBA8Unorm;
        WGPUTextureUsage texture_usage
            = (WGPUTextureUsage)(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc);
        float min_slope_angle = glm::radians(35.0f); // min slope angle [degrees]
        float max_slope_angle = glm::radians(45.0f); // max slope angle [degrees]
        glm::uvec2 sampling_interval = glm::uvec2(1, 1); // sampling interval in x and y direction [every sampling_interval texels]
        int num_particles_per_cell = 10; // number of particles to release per release cell
        bool use_sph_simulation = true; // whether to use SPH simulation for particle step or simple advection
        float sph_smoothing_length = 8.0f; // smoothing length for SPH simulation
        float sph_particle_mass = 10.0f; // mass of each SPH particle
        float sph_rest_density = 5.0f; // rest density for SPH simulation
        float sph_pressure_stiffness = 15.0f; // pressure stiffness for SPH simulation
        float sph_viscosity = 0.08f; // viscosity for SPH simulation
        float sph_epsilon = 1e-4f; // epsilon for SPH simulation
        float sph_max_speed = 40.0f; // maximum speed for SPH simulation
        bool use_SFLM_simulation = true; // whether to use SFLM simulation instead of SPH for particle step
        float sflm_friction_angle = glm::radians(11.0f); // friction angle for SFLM simulation [degrees]
        float sflm_min_travel_angle = glm::radians(0.1f); // minimum travel angle for SFLM simulation [degrees]
        float sflm_max_velocity = 100.0f; // maximum velocity for SFLM simulation
        float sflm_damping = 0.3f; // damping factor for SFLM simulation
        float sflm_stop_velocity = 0.01f; // velocity threshold for stopping particles in SFLM simulation
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
        float sph_smoothing_length;
        float sph_particle_mass;
        float sph_rest_density;
        float sph_pressure_stiffness;
        float sph_viscosity;
        float sph_epsilon;
        float sph_max_speed;
        float sflm_friction_angle;
        float sflm_min_travel_angle;
        float sflm_max_velocity;
        float sflm_damping;
        float sflm_stop_velocity;
        float padding_0;
        float padding_1;
        float padding_2;
        float padding_3;
    };

    static constexpr size_t PARTICLE_STEP_UNIFORM_ALIGNMENT = 16u;
    static_assert(std::is_standard_layout_v<AvalancheParticleStepSettingsUniform>,
        "AvalancheParticleStepSettingsUniform must be standard-layout for predictable GPU ABI");
    static_assert((sizeof(AvalancheParticleStepSettingsUniform) % PARTICLE_STEP_UNIFORM_ALIGNMENT) == 0u,
        "AvalancheParticleStepSettingsUniform size must be a multiple of 16 bytes for WGSL uniform binding");

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
    static constexpr uint32_t PARTICLE_COMPACTION_INTERVAL_FRAMES = 10u;

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
    std::unique_ptr<webgpu::raii::RawBuffer<float>> m_density_buffer;
    std::unique_ptr<webgpu::raii::RawBuffer<float>> m_pressure_buffer;

    const webgpu::raii::TextureWithSampler* m_cached_normal_texture = nullptr;
    const webgpu::raii::TextureWithSampler* m_cached_height_texture = nullptr;

    glm::uvec2 m_output_dimensions;
    uint32_t m_particle_step_frame_counter = 0u;
    //TODO: other Buffers / outputs for avalanche animation node when implemented (e.g. slope angle texture, etc.)
};
}