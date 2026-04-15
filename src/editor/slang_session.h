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
	GET_SET_PROPERTY(godot::PackedStringArray, search_paths)
	GET_SET_PROPERTY(godot::Dictionary, preprocessor_macros)
	GET_SET_PROPERTY(bool, enable_glsl)
	GET_SET_PROPERTY(ShaderTypeLayoutShape::MatrixLayout, default_matrix_layout)

protected:
	static void _bind_methods();

public:
	SlangSession();

	slang::ISession* get_or_create_session();

	godot::Ref<SlangModule> load_module_from_source_string(const godot::String& module_name, const godot::String& path, const godot::String& source_text);
	godot::Ref<SlangModule> load_module_from_source_file(const godot::String& module_name, const godot::String& path);

	godot::Ref<SlangComponentType> create_composite_component_type(const godot::TypedArray<SlangComponentType>& component_types);

	static godot::Ref<SlangSession> create_default_session();
	static godot::String get_builtin_modules_path();
	static godot::PackedStringArray get_additional_search_paths();
	static godot::Dictionary get_builtin_macros();

private:
	Slang::ComPtr<slang::ISession> session;

	static slang::IGlobalSession* _get_global_session(bool enable_glsl = false);

};

}