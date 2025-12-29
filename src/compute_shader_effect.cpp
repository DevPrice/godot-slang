#include "compute_shader_effect.h"

#include "godot_cpp/classes/editor_file_system.hpp"
#include "godot_cpp/classes/editor_interface.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/render_data.hpp"
#include "godot_cpp/classes/render_scene_buffers_rd.hpp"
#include "godot_cpp/classes/render_scene_data.hpp"
#include "godot_cpp/classes/rendering_server.hpp"

#include "attributes.h"

void ComputeShaderEffect::_bind_methods() {
	ADD_SIGNAL(MethodInfo("uniforms_bound",  PropertyInfo(Variant::INT, "view")));
	BIND_GET_SET_RESOURCE(ComputeShaderEffect, task, ComputeShaderTask);
	BIND_METHOD(ComputeShaderEffect, queue_dispatch, "kernel_name");
	GDVIRTUAL_BIND(_bind_view, "task", "kernel", "render_data", "view");
}

Ref<ComputeShaderTask> ComputeShaderEffect::get_task() const { return task; }

void ComputeShaderEffect::set_task(Ref<ComputeShaderTask> p_task) {
	if (p_task != task) {
		const Callable changed_callable = callable_mp(this, &ComputeShaderEffect::_task_changed);
		if (task.is_valid() && task->is_connected("changed", changed_callable)) {
			task->disconnect("changed", changed_callable);
		}
		task = p_task;
		if (p_task.is_valid()) {
			p_task->connect("changed", changed_callable);
		}
		_task_changed();
	}
}

void ComputeShaderEffect::_render_callback(const int32_t p_effect_callback_type, RenderData* p_render_data) {
	CompositorEffect::_render_callback(p_effect_callback_type, p_render_data);
	if (task.is_null()) {
		return;
	}
	const TypedArray<ComputeShaderKernel> kernels = task->get_kernels();
	if (kernels.is_empty()) return;

	if (is_first_run) {
		is_first_run = false;
		queued_kernels.clear();
		for (const Ref<ComputeShaderKernel> kernel : kernels) {
			if (kernel->get_user_attributes().has(CompositorAttributes::once())) {
				queue_dispatch(kernel->get_kernel_name());
			}
		}
	}

	auto* render_scene_buffers = cast_to<RenderSceneBuffersRD>(p_render_data->get_render_scene_buffers().ptr());
	ERR_FAIL_NULL(render_scene_buffers);
	const Vector2i size = render_scene_buffers->get_internal_size();
	ERR_FAIL_COND(size.x <= 0 || size.y <= 0);

	const uint32_t view_count = render_scene_buffers->get_view_count();
	const uint64_t kernel_count = kernels.size();
	for (int32_t kernel_index = 0; kernel_index < kernel_count; ++kernel_index) {
		Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
		const Dictionary kernel_attributes = kernel->get_user_attributes();
		if (kernel.is_valid() && kernel->get_compile_error().is_empty()) {
			if (!queued_kernels.erase(kernel->get_kernel_name()) && (kernel_attributes.has(CompositorAttributes::skip()) || kernel_attributes.has(CompositorAttributes::once()))) {
				continue;
			}
			const Vector3i local_size = kernel->get_thread_group_size();
			ERR_FAIL_COND(local_size.x == 0 || local_size.y == 0 || local_size.z == 0);
			const Vector3i groups(
					(size.x - 1) / local_size.x + 1,
					(size.y - 1) / local_size.y + 1,
					1);
			for (int32_t view = 0; view < view_count; ++view) {
				_bind_parameters(task, kernel, p_render_data->get_render_scene_data(), render_scene_buffers, view);
				GDVIRTUAL_CALL(_bind_view, task, kernel, p_render_data, view);
				emit_signal("uniforms_bound", view);
				task->dispatch_at(kernel_index, groups);
			}
		}
	}
}

void ComputeShaderEffect::_task_changed() {
	is_first_run = true;

	// Work around render callback not being called?
	set_effect_callback_type(get_effect_callback_type());
}

void ComputeShaderEffect::_bind_parameters(const Ref<ComputeShaderTask>& task, const Ref<ComputeShaderKernel>& kernel, const RenderSceneData* scene_data, RenderSceneBuffersRD* render_scene_buffers, const int32_t view) {
	const Dictionary params = kernel->get_parameters();
	Array param_names = params.keys();
	for (const StringName param_name : param_names) {
		if (!param_name.is_empty()) {
			static StringName key_context("context");
			static StringName key_name("name");
			Dictionary param_dict = params.get(param_name, Dictionary());
			Dictionary user_attributes = param_dict.get("user_attributes", Dictionary());
			if (user_attributes.has(CompositorAttributes::internal_size())) {
				task->set_shader_parameter(param_name, render_scene_buffers->get_internal_size());
			}
			if (user_attributes.has(CompositorAttributes::scene_data())) {
				task->set_shader_parameter(param_name, scene_data->get_uniform_buffer());
			}
			if (user_attributes.has(CompositorAttributes::color_texture())) {
				task->set_shader_parameter(param_name, render_scene_buffers->get_color_layer(view));
			}
			if (user_attributes.has(CompositorAttributes::depth_texture())) {
				task->set_shader_parameter(param_name, render_scene_buffers->get_depth_layer(view));
			}
			if (user_attributes.has(CompositorAttributes::velocity_texture())) {
				task->set_shader_parameter(param_name, render_scene_buffers->get_velocity_layer(view));
			}
			if (user_attributes.has(CompositorAttributes::scene_buffer())) {
				Dictionary args = user_attributes[CompositorAttributes::scene_buffer()];
				const String context = args.get(key_context, String());
				const String name = args.get(key_name, String());
				task->set_shader_parameter(param_name, render_scene_buffers->get_texture(context, name));
			}
			if (user_attributes.has(CompositorAttributes::texture())) {
				Dictionary args = user_attributes[CompositorAttributes::texture()];
				Dictionary texture_name_attribute = user_attributes.get(CompositorAttributes::texture_name(), Dictionary());
				const int32_t format = args.get("format", 0);
				const String texture_name = texture_name_attribute.get(key_name, param_name);
				// TODO: Make more of this configurable
				const RID texture = render_scene_buffers->create_texture(
					"__global_context",
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
			} else if (user_attributes.has(CompositorAttributes::texture_name())) {
				Dictionary args = user_attributes[CompositorAttributes::texture_name()];
				const String texture_name = args.get(key_name, String());
				const RID texture = render_scene_buffers->get_texture("__global_context", texture_name);
				task->set_shader_parameter(param_name, texture);
			}
		}
	}
}

void ComputeShaderEffect::queue_dispatch(const String& kernel_name) {
	queued_kernels.set(kernel_name, true);
}
