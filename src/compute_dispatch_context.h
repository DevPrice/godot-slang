#pragma once

#include "binding_macros.h"

#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/render_data.hpp"

class ComputeDispatchContext : public godot::RefCounted {
    GDCLASS(ComputeDispatchContext, RefCounted);

    GET_SET_PROPERTY(godot::RenderData*, render_data)

protected:
    static void _bind_methods();

public:
    ComputeDispatchContext() = default;

};
