#pragma once

#include "slang.h"

#include "godot_cpp/classes/ref_counted.hpp"

#include "binding_macros.h"
#include "compute_shader_shape.h"
#include "slang-com-ptr.h"

namespace gdslang {

class SlangSession final : public godot::RefCounted {
	GDCLASS(SlangSession, RefCounted)

	GET_SET_PROPERTY(godot::String, profile)
	GET_SET_PROPERTY(bool, enable_glsl)
	GET_SET_PROPERTY(ShaderTypeLayoutShape::MatrixLayout, default_matrix_layout)

protected:
	static void _bind_methods();

public:
	SlangSession();

	slang::ISession* get_or_create_session();

private:
	Slang::ComPtr<slang::ISession> session;

	static slang::IGlobalSession* _get_global_session(bool enable_glsl = false);

};

}