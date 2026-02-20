#include "compute_dispatch_context.h"

using namespace godot;

void ComputeDispatchContext::_bind_methods() {
    BIND_GET_SET_METHOD(ComputeDispatchContext, render_data)
}

GET_SET_PROPERTY_IMPL(ComputeDispatchContext, RenderData*, render_data)
