#include "compute_dispatch_context.h"

using namespace godot;

void CompositorEffectDispatchContext::_bind_methods() {
    BIND_GET_SET_METHOD(CompositorEffectDispatchContext, render_data)
    BIND_GET_SET(CompositorEffectDispatchContext, view, Variant::INT)
}

RenderData* CompositorEffectDispatchContext::get_render_data() const {
    return cast_to<RenderData>(ObjectDB::get_instance(render_data_id));
}

void CompositorEffectDispatchContext::set_render_data(const RenderData* p_render_data) {
    render_data_id = p_render_data ? p_render_data->get_instance_id() : ObjectID{};
}

RenderSceneBuffersRD* CompositorEffectDispatchContext::get_render_scene_buffers() const {
    if (const RenderData* render_data = get_render_data()) {
        return cast_to<RenderSceneBuffersRD>(render_data->get_render_scene_buffers().ptr());
    }
    return nullptr;
}

GET_SET_PROPERTY_IMPL(CompositorEffectDispatchContext, int64_t, view)

void ComputeTextureDispatchContext::_bind_methods() {
	BIND_GET_SET(ComputeTextureDispatchContext, output_size, Variant::VECTOR2I)
	BIND_GET_SET(ComputeTextureDispatchContext, output_texture_rid, Variant::RID)
}

GET_SET_PROPERTY_IMPL(ComputeTextureDispatchContext, Vector2i, output_size)
GET_SET_PROPERTY_IMPL(ComputeTextureDispatchContext, RID, output_texture_rid)
