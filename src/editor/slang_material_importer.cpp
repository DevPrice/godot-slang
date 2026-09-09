#include "slang.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "slang_session.h"

#include "slang_material_importer.h"

using namespace godot;

void SlangMaterialImporter::_bind_methods() {
}

String SlangMaterialImporter::_get_importer_name() const {
	return "slang.material.importer";
}

String SlangMaterialImporter::_get_visible_name() const {
	return "Slang Material";
}

int32_t SlangMaterialImporter::_get_format_version() const {
	return 1;
}

int32_t SlangMaterialImporter::_get_preset_count() const {
	return 1;
}

String SlangMaterialImporter::_get_preset_name(int32_t p_preset_index) const {
	return "Slang Material";
}

PackedStringArray SlangMaterialImporter::_get_recognized_extensions() const {
	return PackedStringArray({ "slang" });
}

TypedArray<Dictionary> SlangMaterialImporter::_get_import_options(const String& p_path, int32_t p_preset_index) const {
	return {};
}

String SlangMaterialImporter::_get_save_extension() const {
	return "res";
}

String SlangMaterialImporter::_get_resource_type() const {
	return Shader::get_class_static();
}

float SlangMaterialImporter::_get_priority() const {
	// Below SlangShaderImporter, so a `.slang` file still imports as a SlangShaderFile unless its
	// author picks this importer.
	return 0.9f;
}

int32_t SlangMaterialImporter::_get_import_order() const {
	return 0;
}

bool SlangMaterialImporter::_get_option_visibility(const String& p_path, const StringName& p_option_name, const Dictionary& p_options) const {
	return true;
}

Error SlangMaterialImporter::_import(const String& p_source_file, const String& p_save_path, const Dictionary& p_options, const TypedArray<String>& p_platform_variants, const TypedArray<String>& p_gen_files) const {
	const Ref<FileAccess> shader_file = FileAccess::open(p_source_file, FileAccess::READ);
	if (shader_file.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	const Ref slang_session = gdslang::SlangSession::create_material_session();
	const Ref<gdslang::SlangModule> module = slang_session->load_module_from_source_string("__main_module", p_source_file.get_file(), shader_file->get_as_text(true));
	ERR_FAIL_NULL_V_MSG(module, ERR_COMPILATION_FAILED, String("[%s] Failed to load module!") % p_source_file);

	String material_error;
	const String material_source = module->compile_material(material_error);
	if (!material_error.is_empty()) {
		UtilityFunctions::push_error(String("[%s] Slang material error:\n%s") % Array({ p_source_file, material_error }));
		return ERR_COMPILATION_FAILED;
	}
	ERR_FAIL_COND_V_MSG(
			material_source.is_empty(),
			ERR_COMPILATION_FAILED,
			String("[%s] No [gd::Material] entry point found! Import this file as a Slang Shader instead.") % p_source_file);

	const Ref shader = memnew(Shader);
	shader->set_code(material_source);

	const String out_filename = p_save_path + String(".") + _get_save_extension();
	return ResourceSaver::get_singleton()->save(shader, out_filename);
}
