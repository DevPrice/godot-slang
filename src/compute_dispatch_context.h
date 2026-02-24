#pragma once

#include "binding_macros.h"

#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/render_data.hpp"
#include "godot_cpp/classes/render_scene_buffers_rd.hpp"

class CompositorEffectDispatchContext : public godot::RefCounted {
    GDCLASS(CompositorEffectDispatchContext, RefCounted);

    GET_SET_PROPERTY(int64_t, view)

protected:
	static void _bind_methods();

private:
	godot::ObjectID render_data_id{};

public:
    CompositorEffectDispatchContext() = default;

    godot::RenderData* get_render_data() const;
    void set_render_data(const godot::RenderData* p_render_data);

    godot::RenderSceneBuffersRD* get_render_scene_buffers() const;
};

class ComputeTextureDispatchContext : public godot::RefCounted {
	GDCLASS(ComputeTextureDispatchContext, RefCounted);

	GET_SET_PROPERTY(godot::RID, output_texture_rid)
	GET_SET_PROPERTY(godot::Vector2i, output_size)

protected:
	static void _bind_methods();

public:
	ComputeTextureDispatchContext() = default;
};
