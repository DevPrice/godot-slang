#include "compute_shader_effect.h"

#include "godot_cpp/classes/editor_file_system.hpp"
#include "godot_cpp/classes/editor_interface.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/render_data.hpp"
#include "godot_cpp/classes/render_scene_buffers_rd.hpp"
#include "godot_cpp/classes/render_scene_data.hpp"
#include "godot_cpp/classes/rendering_server.hpp"

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
			if (kernel_attributes.has("gd_compositor_Skip")) {
				continue;
			}
			if (kernel_attributes.has("gd_compositor_Once")) {
				if (dispatched_kernels.has(kernel->get_kernel_name())) {
					continue;
				}
				dispatched_kernels.set(kernel->get_kernel_name(), true);
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
			if (user_attributes.has("gd_compositor_SceneBuffer")) {
				Dictionary args = user_attributes["gd_compositor_SceneBuffer"];
				const String context = args["context"];
				const String name = args["name"];
				task->set_shader_parameter(param_name, render_scene_buffers->get_texture(context, name));
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
	dispatched_kernels.clear();
	if (compute_shader.is_null()) {
		task.unref();
		return;
	}
	task = Ref(memnew(ComputeShaderTask));
	task->set_kernels(compute_shader->get_kernels());

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
