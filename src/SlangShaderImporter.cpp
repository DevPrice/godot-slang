#include "SlangShaderImporter.h"

#include <SlangShader.h>

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/resource_saver.hpp>

void SlangShaderImporter::_bind_methods() {
	BIND_STATIC_METHOD(SlangShaderImporter, get_editor_setting_slangc_location)
	BIND_STATIC_METHOD(SlangShaderImporter, get_editor_setting_gen_path)
}

String SlangShaderImporter::_get_importer_name() const {
	return "slang.shader.importer";
}

String SlangShaderImporter::_get_visible_name() const {
	return "Slang Compute Shader";
}

int32_t SlangShaderImporter::_get_preset_count() const {
	return 1;
}

String SlangShaderImporter::_get_preset_name(int32_t p_preset_index) const {
	return "Compute Shader";
}

PackedStringArray SlangShaderImporter::_get_recognized_extensions() const {
	return PackedStringArray({"slang"});
}

TypedArray<Dictionary> SlangShaderImporter::_get_import_options(const String &p_path, int32_t p_preset_index) const {
	TypedArray<Dictionary> options{};
	Dictionary default_options{};
	default_options.set("name", "shader_type");
	default_options.set("default_value", 0);
	options.push_back(default_options);
	return options;
}

String SlangShaderImporter::_get_save_extension() const {
	return "tres";
}

String SlangShaderImporter::_get_resource_type() const {
	return "SlangShader";
}

float SlangShaderImporter::_get_priority() const {
	return 1.f;
}

int32_t SlangShaderImporter::_get_import_order() const {
	return 0;
}

bool SlangShaderImporter::_get_option_visibility(const String &p_path, const StringName &p_option_name, const Dictionary &p_options) const {
	return true;
}

Error SlangShaderImporter::_import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options, const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const {
	const Ref<FileAccess> file = FileAccess::open(p_source_file, FileAccess::READ);
	if (file.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	/*const String compiler_path = EditorInterface::get_singleton()->get_editor_settings()->get_setting("");
	Array output{};
	if (const int32_t compile_error = OS::get_singleton()->execute(compiler_path, PackedStringArray(), output)) {
		return static_cast<Error>(compile_error);
	}*/

	Ref slang_shader = memnew(SlangShader);

	String out_filename = p_save_path + String(".") + _get_save_extension();
	return ResourceSaver::get_singleton()->save(slang_shader, out_filename);
}
String SlangShaderImporter::get_editor_setting_slangc_location() {
	const static String slangc_location = "slang/import/compiler";
	return slangc_location;
}
String SlangShaderImporter::get_editor_setting_gen_path() {
	const static String gen_path = "slang/import/generated_glsl_dir";
	return gen_path;
}
