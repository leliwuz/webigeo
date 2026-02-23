#include "ComputeAvalancheAnimationNode.h"

#include <QDebug>

namespace webgpu_engine::compute::nodes {

glm::uvec3 ComputeAvalancheAnimationNode::SHADER_WORKGROUP_SIZE = { 16, 16, 1 };

    ComputeAvalancheAnimationNode::ComputeAvalancheAnimationNode(const PipelineManager& pipeline_manager, WGPUDevice device)
        : ComputeAvalancheAnimationNode(pipeline_manager, device, AvalancheAnimationSettings())
    {
    }

    ComputeAvalancheAnimationNode::ComputeAvalancheAnimationNode(const PipelineManager& pipeline_manager, WGPUDevice device, const AvalancheAnimationSettings& settings)
        : Node(
              {
                    InputSocket(*this, "region aabb", data_type<const radix::geometry::Aabb<2, double>*>()),
                  InputSocket(*this, "normal texture", data_type<const webgpu::raii::TextureWithSampler*>()),
                  InputSocket(*this, "height texture", data_type<const webgpu::raii::TextureWithSampler*>()),
                  InputSocket(*this, "release point texture", data_type<const webgpu::raii::TextureWithSampler*>()),
              },
              {
                  OutputSocket(*this, "storage buffer", data_type<webgpu::raii::RawBuffer<glm::vec4>*>(), [this]() { return m_output_storage_buffer.get(); })
              })
        , m_pipeline_manager { &pipeline_manager }
        , m_device { device }
        , m_queue(wgpuDeviceGetQueue(m_device))
        , m_settings { settings }
        , m_settings_uniform(device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform)
        , m_normal_sampler(create_normal_sampler(m_device))
        , m_height_sampler(create_height_sampler(m_device))
    {
    }

    void ComputeAvalancheAnimationNode::update_gpu_settings(uint32_t run)
    {
        m_settings_uniform.data.min_slope_angle = m_settings.min_slope_angle;
        m_settings_uniform.data.max_slope_angle = m_settings.max_slope_angle;
        m_settings_uniform.data.sampling_interval = m_settings.sampling_interval;
        m_settings_uniform.data.num_particles_per_cell = m_settings.num_particles_per_cell;
        m_settings_uniform.update_gpu_data(m_queue);
    }

    void ComputeAvalancheAnimationNode::set_settings(const AvalancheAnimationSettings& settings) { m_settings = settings; }

    const ComputeAvalancheAnimationNode::AvalancheAnimationSettings& ComputeAvalancheAnimationNode::get_settings() const { return m_settings; }

    void ComputeAvalancheAnimationNode::run_impl()
    {
        qDebug() << "running ComputeAvalancheAnimationNode ...";

        auto& region_socket = input_socket("region aabb");
        auto& normal_socket = input_socket("normal texture");
        auto& height_socket = input_socket("height texture");
        auto& release_socket = input_socket("release point texture");

        if (!region_socket.is_socket_connected() || !normal_socket.is_socket_connected()
        || !height_socket.is_socket_connected() || !release_socket.is_socket_connected())
    {
        emit run_failed(NodeRunFailureInfo(*this, "missing required input connections"));
        return;
    }

        const auto region_aabb = std::get<data_type<const radix::geometry::Aabb<2, double>*>()>(region_socket.get_connected_data());
        const auto& normal_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(normal_socket.get_connected_data());
        const auto& height_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(height_socket.get_connected_data());
        const auto& release_point_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(release_socket.get_connected_data());
    
        const auto input_width = normal_texture.texture().width();
        const auto input_height = normal_texture.texture().height();
    
        // assert input textures have same size, otherwise fail run
        if (input_width != height_texture.texture().width() || input_height != height_texture.texture().height()
            || input_width != release_point_texture.texture().width() || input_height != release_point_texture.texture().height()) {
            emit run_failed(NodeRunFailureInfo(*this,
                std::format("failed to compute animation: input texture sizes must match (normals: {}x{}, heights: {}x{}, release points: {}x{})", input_width,
                    input_height, height_texture.texture().width(), height_texture.texture().height(), release_point_texture.texture().width(),
                    release_point_texture.texture().height())));
            return;
        }
        
        m_output_dimensions = glm::uvec2(input_width, input_height) * m_settings.resolution_multiplier;

        qDebug() << "input resolution: " << input_width << "x" << input_height;
        qDebug() << "output resolution: " << m_output_dimensions.x << "x" << m_output_dimensions.y;


        m_output_storage_buffer
        = std::make_unique<webgpu::raii::RawBuffer<glm::vec4>>(m_device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
            m_output_dimensions.x * m_output_dimensions.y, "avalanche avalanche compute output storage");

        // create layer buffers
        //TODO

        // update input settings on GPU side
        m_settings_uniform.data.output_resolution = m_output_dimensions;
        m_settings_uniform.data.region_min = glm::fvec2(region_aabb->min);
        m_settings_uniform.data.region_size = glm::fvec2(region_aabb->size());
        update_gpu_settings();

        // create bind group
        std::vector<WGPUBindGroupEntry> entries {
            m_settings_uniform.raw_buffer().create_bind_group_entry(0),
            normal_texture.texture_view().create_bind_group_entry(1),
            height_texture.texture_view().create_bind_group_entry(2),
            release_point_texture.texture_view().create_bind_group_entry(3),
            m_output_storage_buffer->create_bind_group_entry(4),
        };

        webgpu::raii::BindGroup compute_bind_group(
            m_device, m_pipeline_manager->avalanche_animation_compute_bind_group_layout(), entries, "avalanche animation compute bind group");
          
    
        // bind GPU resources and run pipeline´
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "avalanche animation compute command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_device, descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "avalanche animation compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts
                = glm::ceil(glm::vec3(input_width, input_height, m_settings.num_particles_per_cell) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group.handle(), 0, nullptr);
            m_pipeline_manager->avalanche_animation_compute_pipeline().run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "avalanche animation compute command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_queue, 1, &command);
        wgpuCommandBufferRelease(command);

            const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              ComputeAvalancheAnimationNode* _this = reinterpret_cast<ComputeAvalancheAnimationNode*>(userdata);
              emit _this->run_completed(); // emits signal run_finished()
          };

    WGPUQueueWorkDoneCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_work_done,
        .userdata1 = this,
        .userdata2 = nullptr,
    };

    wgpuQueueOnSubmittedWorkDone(m_queue, callback_info);
    }

    std::unique_ptr<webgpu::raii::Sampler> ComputeAvalancheAnimationNode::create_normal_sampler(WGPUDevice device)
    {
        WGPUSamplerDescriptor sampler_desc {};
        sampler_desc.label = WGPUStringView { .data = "avalanche animation normal texture sampler", .length = WGPU_STRLEN };
        sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
        sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
        sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
        sampler_desc.magFilter = WGPUFilterMode_Nearest;
        sampler_desc.minFilter = WGPUFilterMode_Nearest;
        sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
        sampler_desc.lodMinClamp = 0.0f;
        sampler_desc.lodMaxClamp = 1.0f;
        sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
        sampler_desc.maxAnisotropy = 1;
        return std::make_unique<webgpu::raii::Sampler>(device, sampler_desc);
    }

    std::unique_ptr<webgpu::raii::Sampler> ComputeAvalancheAnimationNode::create_height_sampler(WGPUDevice device)
    {
        WGPUSamplerDescriptor sampler_desc {};
        sampler_desc.label = WGPUStringView { .data = "avalanche animation height texture sampler", .length = WGPU_STRLEN };
        sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
        sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
        sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
        sampler_desc.magFilter = WGPUFilterMode_Nearest;
        sampler_desc.minFilter = WGPUFilterMode_Nearest;
        sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
        sampler_desc.lodMinClamp = 0.0f;
        sampler_desc.lodMaxClamp = 1.0f;
        sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
        sampler_desc.maxAnisotropy = 1;
        return std::make_unique<webgpu::raii::Sampler>(device, sampler_desc);
    }

} // namespace webgpu_engine::compute::nodes