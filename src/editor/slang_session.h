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

	using SlangFormat = SlangCompileTarget;
	enum SlangCompileTarget : int64_t;

	GET_SET_PROPERTY(SlangCompileTarget, format)
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

public:
	enum SlangCompileTarget : int64_t {
        SLANG_TARGET_UNKNOWN,
        SLANG_TARGET_NONE,
        SLANG_GLSL,
        SLANG_GLSL_VULKAN_DEPRECATED,
        SLANG_GLSL_VULKAN_ONE_DESC_DEPRECATED,
        SLANG_HLSL,
        SLANG_SPIRV,
        SLANG_SPIRV_ASM,
        SLANG_DXBC,
        SLANG_DXBC_ASM,
        SLANG_DXIL,
        SLANG_DXIL_ASM,
        SLANG_C_SOURCE,
        SLANG_CPP_SOURCE,
        SLANG_HOST_EXECUTABLE,
        SLANG_SHADER_SHARED_LIBRARY,
        SLANG_SHADER_HOST_CALLABLE,
        SLANG_CUDA_SOURCE,
        SLANG_PTX,
        SLANG_CUDA_OBJECT_CODE,
        SLANG_OBJECT_CODE,
        SLANG_HOST_CPP_SOURCE,
        SLANG_HOST_HOST_CALLABLE,
        SLANG_CPP_PYTORCH_BINDING,
        SLANG_METAL,
        SLANG_METAL_LIB,
        SLANG_METAL_LIB_ASM,
        SLANG_HOST_SHARED_LIBRARY,
        SLANG_WGSL,
        SLANG_WGSL_SPIRV_ASM,
        SLANG_WGSL_SPIRV,
        SLANG_HOST_VM,
        SLANG_CPP_HEADER,
        SLANG_CUDA_HEADER,
        SLANG_HOST_OBJECT_CODE,
        SLANG_HOST_LLVM_IR,
        SLANG_SHADER_LLVM_IR,
        SLANG_TARGET_COUNT_OF,
    };

};

}

VARIANT_ENUM_CAST(gdslang::SlangSession::SlangCompileTarget)
