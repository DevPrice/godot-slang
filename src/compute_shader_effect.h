#pragma once

#include "godot_cpp/classes/compositor_effect.hpp"
#include "godot_cpp/classes/render_data.hpp"
#include "godot_cpp/classes/render_scene_buffers_rd.hpp"
#include <godot_cpp/core/gdvirtual.gen.inc>

#include "binding_macros.h"
#include "compute_shader_file.h"
#include "compute_shader_task.h"

using namespace godot;

class ComputeShaderEffect : public CompositorEffect {
	GDCLASS(ComputeShaderEffect, CompositorEffect)

	GET_SET_PROPERTY(Ref<ComputeShaderFile>, compute_shader)

protected:
	static void _bind_methods();

public:
	void _render_callback(int32_t p_effect_callback_type, RenderData* p_render_data) override;

	[[nodiscard]] Ref<ComputeShaderTask> get_task() const;

	void reload_shader();
	void queue_dispatch(const String& kernel_name);

	bool _set(const StringName& p_name, const Variant& p_value);
	bool _get(const StringName& p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo>* p_list) const;
	[[nodiscard]] bool _property_can_revert(const StringName& p_name) const;
	bool _property_get_revert(const StringName& p_name, Variant& r_property) const;

	GDVIRTUAL4(_bind_view, Ref<ComputeShaderTask>, Ref<ComputeShaderKernel>, RenderData*, int32_t)

private:
	Ref<ComputeShaderTask> task;
	Dictionary queued_kernels;

	static void _bind_parameters(const Ref<ComputeShaderTask>& task, const Ref<ComputeShaderKernel>& kernel, const RenderSceneData* scene_data, RenderSceneBuffersRD* render_scene_buffers, int32_t view);
	void _resources_reimported(const PackedStringArray& resources);
};
