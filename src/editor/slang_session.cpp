#include "slang_session.h"

#include "enums.h"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "slang_shader_editor_plugin.h"

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
		for (const Variant& key : macros.keys()) {
			char_strings.reserve(macros.size() * 2);
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
	BIND_GET_SET(SlangSession, profile, Variant::STRING);
	BIND_GET_SET(SlangSession, search_paths, Variant::PACKED_STRING_ARRAY);
	BIND_GET_SET(SlangSession, preprocessor_macros, Variant::DICTIONARY);
	BIND_GET_SET_ENUM(SlangSession, default_matrix_layout, ENUM_HINT_STRING(ShaderTypeLayoutShape, MatrixLayout));
	BIND_GET_SET(SlangSession, enable_glsl, Variant::BOOL);
	BIND_METHOD(SlangSession, load_module_from_source_file, "module_name", "path");
	BIND_METHOD(SlangSession, load_module_from_source_string, "module_name", "path", "source_text");
}

gdslang::SlangSession::SlangSession() : profile("spirv_1_5"), default_matrix_layout(ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR) { }

slang::ISession* gdslang::SlangSession::get_or_create_session() {
	if (session) {
		return session.get();
	}
	slang::IGlobalSession* global_session = _get_global_session(enable_glsl);
	ERR_FAIL_NULL_V_MSG(global_session, nullptr, "Failed to initialize global Slang session!");

	slang::SessionDesc session_desc = {};
	slang::TargetDesc target_desc = {};
	target_desc.format = SLANG_SPIRV;
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
		slang::IModule* module_ptr = session_ptr->loadModuleFromSourceString(
				module_name.utf8().get_data(),
				path.utf8().get_data(),
				source_text.utf8().get_data(),
				diagnostics_blob.writeRef());
		if (module_ptr) {
			module->set_module(module_ptr);
		}
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
