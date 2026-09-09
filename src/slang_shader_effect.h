#pragma once

#include "godot_cpp/classes/compositor_effect.hpp"
#include "godot_cpp/classes/render_data.hpp"
#include <godot_cpp/core/gdvirtual.gen.inc>

#include "binding_macros.h"
#include "slang_shader_task.h"

class SlangShaderEffect : public godot::CompositorEffect {
	GDCLASS(SlangShaderEffect, CompositorEffect)

	GET_SET_PROPERTY(godot::Ref<SlangShaderTask>, task)

protected:
	static void _bind_methods();

public:
	void _render_callback(int32_t p_effect_callback_type, godot::RenderData* p_render_data) override;

	void queue_dispatch(const godot::String& kernel_name);

	GDVIRTUAL4(_bind_view, godot::Ref<SlangShaderTask>, godot::Ref<SlangShaderProgram>, godot::RenderData*, int32_t)

private:
	godot::Dictionary queued_kernels;
	bool is_first_run = true;

	void _task_changed();
};
