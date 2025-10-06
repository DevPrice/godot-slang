#include "compute_texture.h"

#include "godot_cpp/classes/editor_file_system.hpp"
#include "godot_cpp/classes/editor_interface.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/rd_texture_format.hpp"
#include "godot_cpp/classes/rd_texture_view.hpp"

void ComputeTexture::_bind_methods() {
    BIND_METHOD(ComputeTexture, get_task)
	BIND_METHOD(ComputeTexture, reload_shader)
    BIND_METHOD(ComputeTexture, render)
    BIND_GET_SET(ComputeTexture, size, Variant::VECTOR2I)
    BIND_GET_SET(ComputeTexture, is_animated, Variant::BOOL)
    BIND_GET_SET_RESOURCE(ComputeTexture, compute_shader, ComputeShaderFile)
    BIND_GET_SET_METHOD(ComputeTexture, data_format)
}

ComputeTexture::ComputeTexture() {
    size = Size2i(256, 256);
    data_format = RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM;
}

ComputeTexture::~ComputeTexture() {
    if (RenderingServer* rendering_server = RenderingServer::get_singleton()) {
        if (texture_rid.is_valid()) {
            rendering_server->free_rid(texture_rid);
        }
        if (texture_rd_rid.is_valid()) {
            if (RenderingDevice* rd = rendering_server->get_rendering_device()) {
                rd->free_rid(texture_rd_rid);
            }
        }
        const Callable callable = callable_mp(this, &ComputeTexture::render);
        if (rendering_server->is_connected("frame_pre_draw", callable)) {
            rendering_server->disconnect("frame_pre_draw", callable);
        }
    }
}

Size2i ComputeTexture::get_size() const { return size; }

void ComputeTexture::set_size(const Size2i p_size) {
    ERR_FAIL_COND(p_size.x <= 0 || p_size.y <= 0);
    if (p_size != size) {
        size = p_size;
        _queue_update();
    }
}

RenderingDevice::DataFormat ComputeTexture::get_data_format() const { return data_format; }

void ComputeTexture::set_data_format(const RenderingDevice::DataFormat p_data_format) {
    ERR_FAIL_INDEX(p_data_format, RenderingDevice::DATA_FORMAT_MAX);
    if (p_data_format != data_format) {
        data_format = p_data_format;
        _queue_update();
    }
}

Ref<ComputeShaderFile> ComputeTexture::get_compute_shader() const {
    return compute_shader;
}

void ComputeTexture::set_compute_shader(Ref<ComputeShaderFile> p_compute_shader) {
    compute_shader = p_compute_shader;
    RenderingServer::get_singleton()->call_on_render_thread(callable_mp(this, &ComputeTexture::reload_shader));
}

int ComputeTexture::_get_width() const {
    return size.width;
}

int ComputeTexture::_get_height() const {
    return size.height;
}

bool ComputeTexture::get_is_animated() const { return is_animated; }

void ComputeTexture::set_is_animated(const bool p_is_animated) {
    if (is_animated != p_is_animated) {
        is_animated = p_is_animated;
        const Callable callable = callable_mp(this, &ComputeTexture::render);
        const bool is_connected = RenderingServer::get_singleton()->is_connected("frame_pre_draw", callable);
        if (p_is_animated && !is_connected) {
            RenderingServer::get_singleton()->connect("frame_pre_draw", callable);
        } else if (!p_is_animated && is_connected) {
            RenderingServer::get_singleton()->disconnect("frame_pre_draw", callable);
        }
    }
}

RID ComputeTexture::_get_rid() const {
    return texture_rid;
}

bool ComputeTexture::_has_alpha() const {
    return false;
}

Ref<ComputeShaderTask> ComputeTexture::get_task() const {
    return task;
}

void ComputeTexture::render() {
    if (task.is_null() || !texture_rd_rid.is_valid()) return;

    const TypedArray<ComputeShaderKernel> kernels = task->get_kernels();
    const uint64_t kernel_count = kernels.size();
    for (int32_t kernel_index = 0; kernel_index < kernel_count; ++kernel_index) {
        Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
        const Dictionary kernel_attributes = kernel->get_user_attributes();
        if (kernel.is_valid()) {
            const Vector3i local_size = kernel->get_thread_group_size();
            const Vector3i groups(
                    (get_width() - 1) / local_size.x + 1,
                    (get_height() - 1) / local_size.y + 1,
                    1);
            _bind_parameters(task, kernel);
            task->dispatch_at(kernel_index, groups);
        }
    }
}

void ComputeTexture::_bind_parameters(const Ref<ComputeShaderTask>& p_task, const Ref<ComputeShaderKernel>& p_kernel) const {
    const Dictionary params = p_kernel->get_parameters();
    for (const StringName param_name : params.keys()) {
        if (!param_name.is_empty()) {
            Dictionary param_dict = params[param_name];
            Dictionary user_attributes = param_dict["user_attributes"];
            if (user_attributes.has("gd_texture_TextureSize")) {
                p_task->set_shader_parameter(param_name, size);
            }
            if (user_attributes.has("gd_texture_OutputTexture")) {
                task->set_shader_parameter(param_name, texture_rd_rid);
            }
        }
    }
}

void ComputeTexture::_resources_reimported(const PackedStringArray& resources) {
    if (compute_shader.is_valid() && resources.has(compute_shader->get_path())) {
        RenderingServer::get_singleton()->call_on_render_thread(callable_mp(this, &ComputeTexture::reload_shader));
    }
}

void ComputeTexture::_queue_update() {
    if (updated_queued) return;
    updated_queued = true;
    RenderingServer* rendering_server = RenderingServer::get_singleton();
    ERR_FAIL_NULL(rendering_server);
    rendering_server->call_on_render_thread(callable_mp(this, &ComputeTexture::_update_textures));
}

void ComputeTexture::_update_textures() {
    updated_queued = false;
    if (compute_shader.is_null()) return;

    const Size2i new_size = size;
    const RenderingDevice::DataFormat new_format = data_format;

    if (new_size == remote_size && new_format == remote_data_format) return;

    RenderingServer* rendering_server = RenderingServer::get_singleton();
    ERR_FAIL_NULL(rendering_server);
    ERR_FAIL_COND(!rendering_server->is_on_render_thread());

    RenderingDevice* rd = rendering_server->get_rendering_device();
    ERR_FAIL_NULL(rd);

    Ref<RDTextureFormat> format;
    format.instantiate();
    format->set_width(new_size.x);
    format->set_height(new_size.y);
    format->set_format(new_format);
    format->set_texture_type(RenderingDevice::TEXTURE_TYPE_2D);
    format->set_usage_bits(RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_STORAGE_BIT);

    Ref<RDTextureView> view;
    view.instantiate();

    if (texture_rd_rid.is_valid()) {
        // TODO: This gives an invalid RID error, but leaks if I don't free
        rd->free_rid(texture_rd_rid);
    }

    texture_rd_rid = rd->texture_create(format, view);

    if (texture_rid.is_valid()) {
        rendering_server->texture_replace(texture_rid, rendering_server->texture_rd_create(texture_rd_rid));
    } else {
        texture_rid = rendering_server->texture_rd_create(texture_rd_rid);
    }

    remote_size = new_size;
    remote_data_format = new_format;
}

void ComputeTexture::reload_shader() {
    if (compute_shader.is_null()) {
        task.unref();
        return;
    }
    if (task.is_null() || !task->has_meta("compute_shader") || compute_shader != task->get_meta("compute_shader")) {
        task = Ref(memnew(ComputeShaderTask));
        task->set_meta("compute_shader", compute_shader);
    }
    const TypedArray<ComputeShaderKernel> kernels = compute_shader->get_kernels();
    task->set_kernels(kernels);

    notify_property_list_changed();

#ifdef TOOLS_ENABLED
    if (Engine::get_singleton()->is_editor_hint()) {
        if (const EditorInterface* editor_interface = EditorInterface::get_singleton()) {
            if (EditorFileSystem* editor_fs = editor_interface->get_resource_filesystem()) {
                const Callable callable = callable_mp(this, &ComputeTexture::_resources_reimported);
                if (!editor_fs->is_connected("resources_reimported", callable)) {
                    editor_fs->connect("resources_reimported", callable, CONNECT_ONE_SHOT);
                }
            }
        }
    }
#endif

    _queue_update();
    if (!get_is_animated()) {
        if (RenderingServer* rendering_server = RenderingServer::get_singleton()) {
            rendering_server->call_on_render_thread(callable_mp(this, &ComputeTexture::render));
        }
    }
}
