#pragma once

#include <godot_cpp/classes/texture2d.hpp>

#include "binding_macros.h"
#include "compute_shader_file.h"
#include "compute_shader_task.h"

using namespace godot;

class ComputeTexture final : public Texture2D {
    GDCLASS(ComputeTexture, Texture2D)

    GET_SET_PROPERTY(Ref<ComputeShaderFile>, compute_shader)
    GET_SET_PROPERTY(Size2i, size)
    GET_SET_PROPERTY(RenderingDevice::DataFormat, data_format)
    GET_SET_PROPERTY(bool, is_animated)

protected:
    static void _bind_methods();

public:
    ComputeTexture();
    ~ComputeTexture() override;

    [[nodiscard]] int _get_width() const override;
    [[nodiscard]] int _get_height() const override;
    [[nodiscard]] RID _get_rid() const override;
    [[nodiscard]] bool _has_alpha() const override;

    [[nodiscard]] Ref<ComputeShaderTask> get_task() const;

    void render();
    void reload_shader();

private:
    RID texture_rid{};
    RID texture_rd_rid{};
    Ref<ComputeShaderTask> task;
    Size2i remote_size{};
    RenderingDevice::DataFormat remote_data_format{};
    bool updated_queued{};

    void _bind_parameters(const Ref<ComputeShaderTask>& p_task, const Ref<ComputeShaderKernel>& p_kernel) const;
    void _resources_reimported(const PackedStringArray& resources);
    void _queue_update();
    void _update_textures();
};
