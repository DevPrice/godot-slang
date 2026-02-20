#include "compute_dispatch_context.h"

using namespace godot;

void ComputeDispatchContext::_bind_methods() {
    BIND_GET_SET_METHOD(ComputeDispatchContext, render_data)
    BIND_GET_SET(ComputeDispatchContext, view, Variant::INT)
}

RenderData* ComputeDispatchContext::get_render_data() const {
    return cast_to<RenderData>(ObjectDB::get_instance(render_data_id));
}

void ComputeDispatchContext::set_render_data(const RenderData* p_render_data) {
    render_data_id = p_render_data ? p_render_data->get_instance_id() : ObjectID{};
}

RenderSceneBuffersRD* ComputeDispatchContext::get_render_scene_buffers() const {
    if (const RenderData* render_data = get_render_data()) {
        return cast_to<RenderSceneBuffersRD>(render_data->get_render_scene_buffers().ptr());
    }
    return nullptr;
}

GET_SET_PROPERTY_IMPL(ComputeDispatchContext, int64_t, view)
