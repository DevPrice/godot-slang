#pragma once

#include <godot_cpp/classes/texture2d.hpp>

#include "binding_macros.h"
#include "slang_shader_file.h"
#include "slang_shader_task.h"

class SlangShaderTexture final : public godot::Texture2D {
    GDCLASS(SlangShaderTexture, Texture2D)

    GET_SET_PROPERTY(godot::Ref<SlangShaderTask>, task)
    GET_SET_PROPERTY(godot::Size2i, size)
    GET_SET_PROPERTY(godot::RenderingDevice::DataFormat, data_format)
    GET_SET_PROPERTY(bool, is_animated)

protected:
    static void _bind_methods();

public:
    SlangShaderTexture();
    ~SlangShaderTexture() override;

    [[nodiscard]] int _get_width() const override;
    [[nodiscard]] int _get_height() const override;
    [[nodiscard]] godot::RID _get_rid() const override;
    [[nodiscard]] bool _has_alpha() const override;

    void render();

private:
    UniqueRID<godot::RenderingServer> texture_rid;
    UniqueRID<godot::RenderingDevice> texture_rd_rid;
    godot::Size2i remote_size{};
    godot::RenderingDevice::DataFormat remote_data_format{};
    bool updated_queued{};

    void _queue_update();
    void _update_textures();
	void _task_changed();
};
