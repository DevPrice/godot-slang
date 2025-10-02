#include "compute_shader_effect.h"

#include "godot_cpp/classes/editor_file_system.hpp"
#include "godot_cpp/classes/editor_interface.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/render_data.hpp"
#include "godot_cpp/classes/render_scene_buffers_rd.hpp"
#include "godot_cpp/classes/render_scene_data.hpp"
#include "godot_cpp/classes/rendering_server.hpp"

struct Attributes {
	static StringName& once() {
		static StringName attribute_once("gd_compositor_Once");
		return attribute_once;
	}
	static StringName& skip() {
		static StringName attribute_once("gd_compositor_Skip");
		return attribute_once;
	}
	static StringName& texture() {
		static StringName attribute_once("gd_compositor_Texture");
		return attribute_once;
	}
	static StringName& texture_name() {
		static StringName attribute_once("gd_compositor_TextureName");
		return attribute_once;
	}
	static StringName& scene_buffer() {
		static StringName attribute_once("gd_compositor_SceneBuffer");
		return attribute_once;
	}
};

void ComputeShaderEffect::_bind_methods() {
	ADD_SIGNAL(MethodInfo("uniforms_bound",  PropertyInfo(Variant::INT, "view")));
	BIND_GET_SET_RESOURCE(ComputeShaderEffect, compute_shader, ComputeShaderFile);
	BIND_METHOD(ComputeShaderEffect, get_task);
	BIND_METHOD(ComputeShaderEffect, reload_shader);
	GDVIRTUAL_BIND(_bind_view, "task", "kernel", "render_data", "view");
}

Ref<ComputeShaderFile> ComputeShaderEffect::get_compute_shader() const {
	return compute_shader;
}

void ComputeShaderEffect::set_compute_shader(Ref<ComputeShaderFile> p_compute_shader) {
	compute_shader = p_compute_shader;
	RenderingServer::get_singleton()->call_on_render_thread(callable_mp(this, &ComputeShaderEffect::reload_shader));
}

void ComputeShaderEffect::_render_callback(const int32_t p_effect_callback_type, RenderData* p_render_data) {
	CompositorEffect::_render_callback(p_effect_callback_type, p_render_data);
	if (task.is_null()) {
		return;
	}
	const Ref<RenderSceneBuffers> render_scene_buffers = p_render_data->get_render_scene_buffers();
	if (render_scene_buffers.is_null()) {
		return;
	}
	auto* rsb = cast_to<RenderSceneBuffersRD>(render_scene_buffers.ptr());
	if (!rsb) {
		return;
	}
	const Vector2i size = rsb->get_internal_size();
	if (size.x <= 0 || size.y <= 0) {
		return;
	}
	const uint32_t view_count = rsb->get_view_count();
	const TypedArray<ComputeShaderKernel> kernels = task->get_kernels();
	const uint64_t kernel_count = kernels.size();
	for (int32_t kernel_index = 0; kernel_index < kernel_count; ++kernel_index) {
		Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
		const Dictionary kernel_attributes = kernel->get_user_attributes();
		if (kernel.is_valid()) {
			if (!queued_kernels.erase(kernel->get_kernel_name()) && (kernel_attributes.has(Attributes::skip()) || kernel_attributes.has(Attributes::once()))) {
				continue;
			}
			const Vector3i local_size = kernel->get_thread_group_size();
			const Vector3i groups(
					(size.x - 1) / local_size.x + 1,
					(size.y - 1) / local_size.y + 1,
					1);
			for (int32_t view = 0; view < view_count; ++view) {
				_bind_parameters(task, kernel, p_render_data->get_render_scene_data(), rsb, view);
				GDVIRTUAL_CALL(_bind_view, task, kernel, p_render_data, view);
				emit_signal("uniforms_bound", view);
				task->dispatch_at(kernel_index, groups);
			}
		}
	}
}

Ref<ComputeShaderTask> ComputeShaderEffect::get_task() const {
	return task;
}

void ComputeShaderEffect::_bind_parameters(const Ref<ComputeShaderTask>& task, const Ref<ComputeShaderKernel>& kernel, const RenderSceneData* scene_data, RenderSceneBuffersRD* render_scene_buffers, const int32_t view) {
	const Dictionary params = kernel->get_parameters();
	Array param_names = params.keys();
	for (uint32_t param_index = 0; param_index < param_names.size(); ++param_index) {
		String param_name = param_names[param_index];
		if (!param_name.is_empty()) {
			static StringName key_context("context");
			static StringName key_name("name");
			Dictionary param_dict = params[param_name];
			Dictionary user_attributes = param_dict["user_attributes"];
			if (user_attributes.has("gd_compositor_Size")) {
				task->set_shader_parameter(param_name, render_scene_buffers->get_internal_size());
			}
			if (user_attributes.has("gd_compositor_SceneData")) {
				task->set_shader_parameter(param_name, scene_data->get_uniform_buffer());
			}
			if (user_attributes.has("gd_compositor_ColorTexture")) {
				task->set_shader_parameter(param_name, render_scene_buffers->get_color_layer(view));
			}
			if (user_attributes.has("gd_compositor_DepthTexture")) {
				task->set_shader_parameter(param_name, render_scene_buffers->get_depth_layer(view));
			}
			if (user_attributes.has("gd_compositor_VelocityTexture")) {
				task->set_shader_parameter(param_name, render_scene_buffers->get_velocity_layer(view));
			}
			if (user_attributes.has(Attributes::scene_buffer())) {
				Dictionary args = user_attributes[Attributes::scene_buffer()];
				const String context = args[key_context];
				const String name = args[key_name];
				task->set_shader_parameter(param_name, render_scene_buffers->get_texture(context, name));
			}
			if (user_attributes.has(Attributes::texture())) {
				Dictionary args = user_attributes[Attributes::texture()];
				Dictionary texture_name_attribute = user_attributes.get(Attributes::texture_name(), Dictionary());
				const int32_t format = args["format"];
				const String texture_name = texture_name_attribute.get(key_name, param_name);
				// TODO: Make more of this configurable
				const RID texture = render_scene_buffers->create_texture(
					kernel->get_kernel_name(),
					texture_name,
					static_cast<RenderingDevice::DataFormat>(format),
					RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_STORAGE_BIT,
					RenderingDevice::TEXTURE_SAMPLES_1,
					render_scene_buffers->get_internal_size(),
					1,
					1,
					false,
					false);
				task->set_shader_parameter(param_name, texture);
			} else if (user_attributes.has(Attributes::texture_name())) {
				Dictionary args = user_attributes[Attributes::texture_name()];
				const String texture_name = args[key_name];
				const RID texture = render_scene_buffers->get_texture(kernel->get_kernel_name(), texture_name);
				task->set_shader_parameter(param_name, texture);
			}
		}
	}
}

void ComputeShaderEffect::_resources_reimported(const PackedStringArray& resources) {
	if (compute_shader.is_valid() && resources.has(compute_shader->get_path())) {
		RenderingServer::get_singleton()->call_on_render_thread(callable_mp(this, &ComputeShaderEffect::reload_shader));
	}
}

void ComputeShaderEffect::reload_shader() {
	if (compute_shader.is_null()) {
		task.unref();
		return;
	}
	const TypedArray<ComputeShaderKernel> kernels = compute_shader->get_kernels();
	task = Ref(memnew(ComputeShaderTask));
	task->set_kernels(kernels);

	queued_kernels.clear();
	for (int32_t i = 0; i < kernels.size(); ++i) {
		const Ref<ComputeShaderKernel> kernel = kernels[i];
		if (kernel->get_user_attributes().has(Attributes::once())) {
			queue_dispatch(kernel->get_kernel_name());
		}
	}

	if (Engine::get_singleton()->is_editor_hint()) {
		if (const EditorInterface* editor_interface = EditorInterface::get_singleton()) {
			if (EditorFileSystem* editor_fs = editor_interface->get_resource_filesystem()) {
				const Callable callable = callable_mp(this, &ComputeShaderEffect::_resources_reimported);
				if (!editor_fs->is_connected("resources_reimported", callable)) {
					editor_fs->connect("resources_reimported", callable, CONNECT_ONE_SHOT);
				}
			}
		}
	}
}

void ComputeShaderEffect::queue_dispatch(const String& kernel_name) {
	queued_kernels.set(kernel_name, true);
}
