#include "compute_shader_effect.h"

#include "godot_cpp/classes/editor_file_system.hpp"
#include "godot_cpp/classes/editor_interface.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/render_data.hpp"
#include "godot_cpp/classes/render_scene_buffers_rd.hpp"
#include "godot_cpp/classes/rendering_server.hpp"

#include "attributes.h"
#include "compute_dispatch_context.h"

using namespace godot;

void ComputeShaderEffect::_bind_methods() {
	ADD_SIGNAL(MethodInfo("view_dispatching", PropertyInfo(Variant::INT, "view")));
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
	if (kernels.is_empty())
		return;

	if (is_first_run) {
		is_first_run = false;
		queued_kernels.clear();
		for (const Ref<ComputeShaderKernel> kernel : kernels) {
			if (kernel->get_user_attributes().has(CompositorAttributes::once())) {
				queue_dispatch(kernel->get_kernel_name());
			}
		}
	}

	const auto* render_scene_buffers = cast_to<RenderSceneBuffersRD>(p_render_data->get_render_scene_buffers().ptr());
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
				GDVIRTUAL_CALL(_bind_view, task, kernel, p_render_data, view);
				Ref<CompositorEffectDispatchContext> dispatch_context;
				dispatch_context.instantiate();
				dispatch_context->set_render_data(p_render_data);
				dispatch_context->set_view(view);
				emit_signal("view_dispatching", view);
				task->dispatch_at(kernel_index, groups, dispatch_context.ptr());
			}
		}
	}
}

void ComputeShaderEffect::_task_changed() {
	is_first_run = true;

	// Work around render callback not being called?
	set_effect_callback_type(get_effect_callback_type());
}

void ComputeShaderEffect::queue_dispatch(const String& kernel_name) {
	queued_kernels.set(kernel_name, true);
}
