#pragma once

#include "binding_macros.h"

#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/render_data.hpp"
#include "godot_cpp/classes/render_scene_buffers_rd.hpp"

class ComputeDispatchContext : public godot::RefCounted {
    GDCLASS(ComputeDispatchContext, RefCounted);

    GET_SET_PROPERTY(int64_t, view)

private:
    godot::ObjectID render_data_id{};

protected:
    static void _bind_methods();

public:
    ComputeDispatchContext() = default;

    godot::RenderData* get_render_data() const;
    void set_render_data(const godot::RenderData* p_render_data);

    godot::RenderSceneBuffersRD* get_render_scene_buffers() const;
};
