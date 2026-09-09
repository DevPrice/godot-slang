#include "slang_shader_context.h"

using namespace godot;

void SlangShaderEffectContext::_bind_methods() {
    BIND_GET_SET_METHOD(SlangShaderEffectContext, render_data)
    BIND_GET_SET(SlangShaderEffectContext, view, Variant::INT)
}

RenderData* SlangShaderEffectContext::get_render_data() const {
    return cast_to<RenderData>(ObjectDB::get_instance(render_data_id));
}

void SlangShaderEffectContext::set_render_data(const RenderData* p_render_data) {
    render_data_id = p_render_data ? p_render_data->get_instance_id() : ObjectID{};
}

RenderSceneBuffersRD* SlangShaderEffectContext::get_render_scene_buffers() const {
    if (const RenderData* render_data = get_render_data()) {
        return cast_to<RenderSceneBuffersRD>(render_data->get_render_scene_buffers().ptr());
    }
    return nullptr;
}

GET_SET_PROPERTY_IMPL(SlangShaderEffectContext, int64_t, view)

void SlangShaderTextureContext::_bind_methods() {
	BIND_GET_SET(SlangShaderTextureContext, output_size, Variant::VECTOR2I)
	BIND_GET_SET(SlangShaderTextureContext, output_texture_rid, Variant::RID)
}

GET_SET_PROPERTY_IMPL(SlangShaderTextureContext, Vector2i, output_size)
GET_SET_PROPERTY_IMPL(SlangShaderTextureContext, RID, output_texture_rid)
