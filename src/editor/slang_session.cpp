#include <vector>

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/project_settings.hpp"

#include "enums.h"
#include "slang_shader_editor_plugin.h"

#include "slang_session.h"

using namespace gdslang;
using namespace godot;

namespace {

class Utf8CharStringArray {
public:
	Utf8CharStringArray(const PackedStringArray& strings) {
		char_strings.reserve(strings.size());
		for (const String& search_path : strings) {
			char_strings.push_back(search_path.utf8());
		}
		string_ptrs.resize(char_strings.size());
		std::ranges::transform(char_strings, string_ptrs.begin(),
			[](const CharString &s) { return s.get_data(); });
	}

	const char* const* data() const { return string_ptrs.data(); }
	size_t size() const { return string_ptrs.size(); }

private:
	std::vector<CharString> char_strings{};
	std::vector<const char*> string_ptrs{};
};

class PreprocessorMacros {
public:
	PreprocessorMacros(const Dictionary& macros) {
		char_strings.reserve(macros.size() * 2);
		for (const Variant& key : macros.keys()) {
			CharString key_string = String(key).utf8();
			CharString value_string = String(macros[key]).utf8();
			char_strings.push_back(key_string);
			char_strings.push_back(value_string);
			preprocessor_macros.push_back({ .name = key_string.get_data(), .value = value_string.get_data() });
		}
	}

	const slang::PreprocessorMacroDesc* data() const { return preprocessor_macros.data(); }
	size_t size() const { return preprocessor_macros.size(); }

private:
	std::vector<CharString> char_strings{};
	std::vector<slang::PreprocessorMacroDesc> preprocessor_macros;
};

}

void gdslang::SlangSession::_bind_methods() {
	BIND_ENUM_CONSTANT(SLANG_TARGET_UNKNOWN)
	BIND_ENUM_CONSTANT(SLANG_TARGET_NONE)
	BIND_ENUM_CONSTANT(SLANG_GLSL)
	BIND_ENUM_CONSTANT(SLANG_GLSL_VULKAN_DEPRECATED)
	BIND_ENUM_CONSTANT(SLANG_GLSL_VULKAN_ONE_DESC_DEPRECATED)
	BIND_ENUM_CONSTANT(SLANG_HLSL)
	BIND_ENUM_CONSTANT(SLANG_SPIRV)
	BIND_ENUM_CONSTANT(SLANG_SPIRV_ASM)
	BIND_ENUM_CONSTANT(SLANG_DXBC)
	BIND_ENUM_CONSTANT(SLANG_DXBC_ASM)
	BIND_ENUM_CONSTANT(SLANG_DXIL)
	BIND_ENUM_CONSTANT(SLANG_DXIL_ASM)
	BIND_ENUM_CONSTANT(SLANG_C_SOURCE)
	BIND_ENUM_CONSTANT(SLANG_CPP_SOURCE)
	BIND_ENUM_CONSTANT(SLANG_HOST_EXECUTABLE)
	BIND_ENUM_CONSTANT(SLANG_SHADER_SHARED_LIBRARY)
	BIND_ENUM_CONSTANT(SLANG_SHADER_HOST_CALLABLE)
	BIND_ENUM_CONSTANT(SLANG_CUDA_SOURCE)
	BIND_ENUM_CONSTANT(SLANG_PTX)
	BIND_ENUM_CONSTANT(SLANG_CUDA_OBJECT_CODE)
	BIND_ENUM_CONSTANT(SLANG_OBJECT_CODE)
	BIND_ENUM_CONSTANT(SLANG_HOST_CPP_SOURCE)
	BIND_ENUM_CONSTANT(SLANG_HOST_HOST_CALLABLE)
	BIND_ENUM_CONSTANT(SLANG_CPP_PYTORCH_BINDING)
	BIND_ENUM_CONSTANT(SLANG_METAL)
	BIND_ENUM_CONSTANT(SLANG_METAL_LIB)
	BIND_ENUM_CONSTANT(SLANG_METAL_LIB_ASM)
	BIND_ENUM_CONSTANT(SLANG_HOST_SHARED_LIBRARY)
	BIND_ENUM_CONSTANT(SLANG_WGSL)
	BIND_ENUM_CONSTANT(SLANG_WGSL_SPIRV_ASM)
	BIND_ENUM_CONSTANT(SLANG_WGSL_SPIRV)
	BIND_ENUM_CONSTANT(SLANG_HOST_VM)
	BIND_ENUM_CONSTANT(SLANG_CPP_HEADER)
	BIND_ENUM_CONSTANT(SLANG_CUDA_HEADER)
	BIND_ENUM_CONSTANT(SLANG_HOST_OBJECT_CODE)
	BIND_ENUM_CONSTANT(SLANG_HOST_LLVM_IR)
	BIND_ENUM_CONSTANT(SLANG_SHADER_LLVM_IR)
	BIND_ENUM_CONSTANT(SLANG_TARGET_MAX)
	BIND_GET_SET_ENUM(SlangSession, format, ENUM_HINT_STRING(SlangSession, SlangCompileFormat));
	BIND_GET_SET(SlangSession, profile, Variant::STRING);
	BIND_GET_SET(SlangSession, search_paths, Variant::PACKED_STRING_ARRAY);
	BIND_GET_SET(SlangSession, preprocessor_macros, Variant::DICTIONARY);
	BIND_GET_SET_ENUM(SlangSession, default_matrix_layout, ENUM_HINT_STRING(ShaderTypeLayoutShape, MatrixLayout));
	BIND_GET_SET(SlangSession, enable_glsl, Variant::BOOL);
	BIND_METHOD(SlangSession, load_module_from_source_file, "module_name", "path");
	BIND_METHOD(SlangSession, load_module_from_source_string, "module_name", "path", "source_text");
	BIND_METHOD(SlangSession, create_composite_component_type, "component_types");
	BIND_STATIC_METHOD(SlangSession, create_default_session);
	BIND_STATIC_METHOD(SlangSession, get_builtin_modules_path);
	BIND_STATIC_METHOD(SlangSession, get_builtin_macros);
}

gdslang::SlangSession::SlangSession() : format(SLANG_SPIRV), profile("spirv_1_5"), default_matrix_layout(ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR) { }

slang::ISession* gdslang::SlangSession::get_or_create_session() {
	if (session) {
		return session.get();
	}
	slang::IGlobalSession* global_session = _get_global_session(enable_glsl);
	ERR_FAIL_NULL_V_MSG(global_session, nullptr, "Failed to initialize global Slang session!");

	slang::SessionDesc session_desc = {};
	slang::TargetDesc target_desc = {};
	target_desc.format = static_cast<SlangCompileTarget>(format);
	target_desc.profile = global_session->findProfile(profile.utf8().get_data());

	session_desc.targets = &target_desc;
	session_desc.targetCount = 1;

	session_desc.defaultMatrixLayoutMode = static_cast<SlangMatrixLayoutMode>(default_matrix_layout);

	const Utf8CharStringArray search_paths_array(search_paths);
	session_desc.searchPaths = search_paths_array.data();
	session_desc.searchPathCount = search_paths_array.size();

	const PreprocessorMacros macros(preprocessor_macros);
	session_desc.preprocessorMacroCount = macros.size();
	session_desc.preprocessorMacros = macros.data();

	session_desc.allowGLSLSyntax = enable_glsl;

	ERR_FAIL_COND_V(SLANG_FAILED(global_session->createSession(session_desc, session.writeRef())), nullptr);
	return session.get();
}

Ref<SlangModule> gdslang::SlangSession::load_module_from_source_string(const String& module_name, const String& path, const String& source_text) {
	Ref<SlangModule> module;
	module.instantiate();
	slang::ISession* session_ptr = get_or_create_session();
	ERR_FAIL_NULL_V(session_ptr, module);
	{
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		*module->get_write_ref() = session_ptr->loadModuleFromSourceString(
				module_name.utf8().get_data(),
				path.utf8().get_data(),
				source_text.utf8().get_data(),
				diagnostics_blob.writeRef());
		if (diagnostics_blob) {
			module->set_diagnostic(String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()));
		}
	}
	return module;
}

Ref<SlangModule> gdslang::SlangSession::load_module_from_source_file(const String& module_name, const String& path) {
	const Ref<FileAccess> shader_file = FileAccess::open(path, FileAccess::READ);
	ERR_FAIL_NULL_V_MSG(shader_file, nullptr, String("Failed to open shader file: ") + path);
	return load_module_from_source_string(module_name, path, shader_file->get_as_text(true));
}

Ref<SlangComponentType> gdslang::SlangSession::create_composite_component_type(const TypedArray<SlangComponentType>& component_types) {
	slang::ISession* session_ptr = get_or_create_session();
	ERR_FAIL_NULL_V(session_ptr, nullptr);
	std::vector<slang::IComponentType*> component_type_ptrs;
	component_type_ptrs.reserve(component_types.size());
	for (Ref<SlangComponentType> component_type : component_types) {
		ERR_FAIL_NULL_V(component_type, nullptr);
		slang::IComponentType* ptr = component_type->get_component_type();
		ERR_FAIL_NULL_V(ptr, nullptr);
		component_type_ptrs.push_back(ptr);
	}
	slang::IComponentType* composite;
	Slang::ComPtr<slang::IBlob> diagnostics_blob;
	ERR_FAIL_COND_V(SLANG_FAILED(session_ptr->createCompositeComponentType(component_type_ptrs.data(), component_type_ptrs.size(), &composite, diagnostics_blob.writeRef())), nullptr);
	if (diagnostics_blob) {
		return SlangComponentType::create(composite, String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()));
	}
	return SlangComponentType::create(composite);
}

Ref<gdslang::SlangSession> gdslang::SlangSession::create_default_session() {
	Ref session = memnew(SlangSession);
	PackedStringArray search_paths = get_additional_search_paths();
	search_paths.push_back(get_builtin_modules_path());
	session->set_search_paths(search_paths);
	session->set_preprocessor_macros(get_builtin_macros());
	return session;
}

String gdslang::SlangSession::get_builtin_modules_path() {
	const String extension_path = ProjectSettings::get_singleton()->globalize_path("uid://blqvpxodges3r");
	return extension_path.get_base_dir().path_join("modules");
}

PackedStringArray gdslang::SlangSession::get_additional_search_paths() {
	const ProjectSettings* project_settings = ProjectSettings::get_singleton();
	ERR_FAIL_NULL_V(project_settings, {});
	Array search_paths = project_settings->get_setting("slang/importer/search_paths");
	PackedStringArray globalized_paths{};
	globalized_paths.resize(search_paths.size());
	for (const Variant& path : search_paths) {
		globalized_paths.push_back(project_settings->globalize_path(path));
	}
	return globalized_paths;
}

Dictionary gdslang::SlangSession::get_builtin_macros() {
	Dictionary macros{};
	static StringName godot_major_version_key = "GODOT_MAJOR_VERSION";
	static StringName godot_minor_version_key = "GODOT_MINOR_VERSION";
	const Dictionary version_info = Engine::get_singleton()->get_version_info();
	macros[godot_major_version_key] = version_info.get("major", 0);
	macros[godot_minor_version_key] = version_info.get("minor", 0);
	return macros;
}

slang::IGlobalSession* gdslang::SlangSession::_get_global_session(const bool enable_glsl) {
	static bool glsl_initialized = enable_glsl;
	static Slang::ComPtr<slang::IGlobalSession> global_session = [enable_glsl] {
		Slang::ComPtr<slang::IGlobalSession> ptr;
		SlangGlobalSessionDesc desc = {};
		desc.enableGLSL = enable_glsl;
		slang::createGlobalSession(&desc, ptr.writeRef());
		return ptr;
	}();

	if (!glsl_initialized && enable_glsl) {
		SlangGlobalSessionDesc desc = {};
		desc.enableGLSL = true;
		slang::createGlobalSession(&desc, global_session.writeRef());
		glsl_initialized = true;
	}

	return global_session;
}

gdslang::SlangSession::SlangCompileFormat gdslang::SlangSession::get_format() const { return format; }

void gdslang::SlangSession::set_format(const SlangCompileFormat p_format) {
	ERR_FAIL_COND_MSG(session, "Session may not be modified after loading module(s)!");
	format = p_format;
}

String gdslang::SlangSession::get_profile() const { return profile; }

void gdslang::SlangSession::set_profile(String p_profile) {
	ERR_FAIL_COND_MSG(session, "Session may not be modified after loading module(s)!");
	profile = p_profile;
}

PackedStringArray gdslang::SlangSession::get_search_paths() const { return search_paths; }

void gdslang::SlangSession::set_search_paths(PackedStringArray p_search_paths) {
	ERR_FAIL_COND_MSG(session, "Session may not be modified after loading module(s)!");
	search_paths = p_search_paths;
}

Dictionary gdslang::SlangSession::get_preprocessor_macros() const { return preprocessor_macros; }

void gdslang::SlangSession::set_preprocessor_macros(Dictionary p_preprocessor_macros) {
	ERR_FAIL_COND_MSG(session, "Session may not be modified after loading module(s)!");
	preprocessor_macros = p_preprocessor_macros;
}

bool gdslang::SlangSession::get_enable_glsl() const { return enable_glsl; }

void gdslang::SlangSession::set_enable_glsl(const bool p_enable_glsl) {
	ERR_FAIL_COND_MSG(session, "Session may not be modified after loading module(s)!");
	enable_glsl = p_enable_glsl;
}

ShaderTypeLayoutShape::MatrixLayout gdslang::SlangSession::get_default_matrix_layout() const { return default_matrix_layout; }

void gdslang::SlangSession::set_default_matrix_layout(const ShaderTypeLayoutShape::MatrixLayout p_default_matrix_layout) {
	ERR_FAIL_COND_MSG(session, "Session may not be modified after loading module(s)!");
	default_matrix_layout = p_default_matrix_layout;
}
