#pragma once

#include "slang.h"
#include "slang-com-ptr.h"

#include "godot_cpp/classes/ref_counted.hpp"

#include "binding_macros.h"
#include "compute_shader_shape.h"
#include "slang_module.h"

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

	godot::Ref<SlangModule> load_module_from_source_string(const godot::String& module_name, const godot::String& path, const godot::String& source_text);
	godot::Ref<SlangModule> load_module_from_source_file(const godot::String& module_name, const godot::String& path);

private:
	Slang::ComPtr<slang::ISession> session;

	static slang::IGlobalSession* _get_global_session(bool enable_glsl = false);

};

}