#include "slang_session.h"

#include "enums.h"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/project_settings.hpp"
#include "slang_shader_editor_plugin.h"

using namespace gdslang;
using namespace godot;

void gdslang::SlangSession::_bind_methods() {
	BIND_GET_SET(SlangSession, profile, Variant::STRING);
	BIND_GET_SET_ENUM(SlangSession, default_matrix_layout, ENUM_HINT_STRING(ShaderTypeLayoutShape, MatrixLayout));
	BIND_GET_SET(SlangSession, enable_glsl, Variant::BOOL);
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

	const String extension_path = ProjectSettings::get_singleton()->globalize_path("uid://blqvpxodges3r");
	const CharString modules_path = extension_path.get_base_dir().path_join("modules").utf8();
	const PackedStringArray search_paths = SlangShaderEditorPlugin::get_search_paths();
	std::vector<CharString> search_paths_char_strings;
	search_paths_char_strings.reserve(search_paths.size() + 1);
	search_paths_char_strings.push_back(modules_path);
	for (const String& search_path : search_paths) {
		search_paths_char_strings.push_back(search_path.utf8());
	}
	std::vector<const char*> search_paths_vector;
	search_paths_vector.resize(search_paths_char_strings.size());
	std::ranges::transform(search_paths_char_strings, search_paths_vector.begin(),
		[](const CharString &s) { return s.get_data(); });

	// TODO: Move search paths to property
	session_desc.searchPaths = search_paths_vector.data();
	session_desc.searchPathCount = search_paths_vector.size();

	{
		static auto godot_major_version_key = "GODOT_MAJOR_VERSION";
		static auto godot_minor_version_key = "GODOT_MINOR_VERSION";
		const Dictionary version_info = Engine::get_singleton()->get_version_info();
		static const CharString major_version_string = String::num_int64(version_info.get("major", 0)).utf8();
		static const CharString minor_version_string = String::num_int64(version_info.get("minor", 0)).utf8();
		static const std::array macros = {
			slang::PreprocessorMacroDesc{ .name = godot_major_version_key, .value = major_version_string.get_data() },
			slang::PreprocessorMacroDesc{ .name = godot_minor_version_key, .value = minor_version_string.get_data() },
		};

		// TODO: Move preprocessor macros to property
		session_desc.preprocessorMacroCount = macros.size();
		session_desc.preprocessorMacros = macros.data();
	}

	session_desc.allowGLSLSyntax = enable_glsl;

	ERR_FAIL_COND_V(SLANG_FAILED(global_session->createSession(session_desc, session.writeRef())), nullptr);
	return session.get();
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
	session.setNull();
	profile = p_profile;
}

bool gdslang::SlangSession::get_enable_glsl() const { return enable_glsl; }

void gdslang::SlangSession::set_enable_glsl(const bool p_enable_glsl) {
	session.setNull();
	enable_glsl = p_enable_glsl;
}

ShaderTypeLayoutShape::MatrixLayout gdslang::SlangSession::get_default_matrix_layout() const { return default_matrix_layout; }

void gdslang::SlangSession::set_default_matrix_layout(const ShaderTypeLayoutShape::MatrixLayout p_default_matrix_layout) {
	session.setNull();
	default_matrix_layout = p_default_matrix_layout;
}
