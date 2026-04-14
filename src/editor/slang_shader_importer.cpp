#include "slang_shader_importer.h"

#include <vector>

#include "slang-com-ptr.h"
#include "slang.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_saver.hpp>

#include "attributes.h"
#include <compute_shader_file.h>
#include <compute_shader_kernel.h>

#include "compute_shader_cursor.h"
#include "slang_session.h"
#include "slang_shader_editor_plugin.h"

using namespace godot;

void SlangShaderImporter::_bind_methods() {
}

String SlangShaderImporter::_get_importer_name() const {
	return "slang.shader.importer";
}

String SlangShaderImporter::_get_visible_name() const {
	return "Compute Shader";
}

int32_t SlangShaderImporter::_get_preset_count() const {
	return 1;
}

String SlangShaderImporter::_get_preset_name(int32_t p_preset_index) const {
	return "Compute Shader";
}

PackedStringArray SlangShaderImporter::_get_recognized_extensions() const {
	return PackedStringArray({ "slang", "hlsl", "glsl" });
}

TypedArray<Dictionary> SlangShaderImporter::_get_import_options(const String& p_path, int32_t p_preset_index) const {
	TypedArray<Dictionary> options{};
	{
		Dictionary entry_points_option{};
		entry_points_option.set("name", "entry_points");
		entry_points_option.set("default_value", PackedStringArray());
		options.push_back(entry_points_option);
	}
	{
		Dictionary matrix_layout_option{};
		matrix_layout_option.set("name", "default_matrix_layout");
		matrix_layout_option.set("default_value", ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR);
		matrix_layout_option.set("property_hint", PROPERTY_HINT_ENUM);
		matrix_layout_option.set("hint_string", String("Row-major:%s,Column-major:%s") % Array { String::num_int64(ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR), String::num_int64(ShaderTypeLayoutShape::MatrixLayout::COLUMN_MAJOR) });
		options.push_back(matrix_layout_option);
	}
	return options;
}

String SlangShaderImporter::_get_save_extension() const {
	return "res";
}

String SlangShaderImporter::_get_resource_type() const {
	return "ComputeShaderFile";
}

float SlangShaderImporter::_get_priority() const {
	return 1.f;
}

int32_t SlangShaderImporter::_get_import_order() const {
	return 0;
}

bool SlangShaderImporter::_get_option_visibility(const String& p_path, const StringName& p_option_name, const Dictionary& p_options) const {
	return true;
}

Error SlangShaderImporter::_import(const String& p_source_file, const String& p_save_path, const Dictionary& p_options, const TypedArray<String>& p_platform_variants, const TypedArray<String>& p_gen_files) const {
	const Ref<FileAccess> shader_file = FileAccess::open(p_source_file, FileAccess::READ);
	if (shader_file.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	const String shader_source = shader_file->get_as_text(true);

	const Ref slang_session = memnew(gdslang::SlangSession);
	slang_session->set_enable_glsl(p_source_file.ends_with(".glsl"));
	const int64_t default_matrix_layout = p_options["default_matrix_layout"];
	if (default_matrix_layout >= SLANG_MATRIX_LAYOUT_ROW_MAJOR && default_matrix_layout <= SLANG_MATRIX_LAYOUT_COLUMN_MAJOR) {
		slang_session->set_default_matrix_layout(static_cast<ShaderTypeLayoutShape::MatrixLayout>(default_matrix_layout));
	}

	PackedStringArray search_paths = SlangShaderEditorPlugin::get_search_paths();
	search_paths.push_back(SlangShaderEditorPlugin::get_modules_path());
	slang_session->set_search_paths(search_paths);

	slang_session->set_preprocessor_macros(SlangShaderEditorPlugin::get_preprocessor_macros());

	const Ref<gdslang::SlangModule> module = slang_session->load_module_from_source_string("__main_module", p_source_file.get_file(), shader_source);
	ERR_FAIL_NULL_V_MSG(module, ERR_COMPILATION_FAILED, String("[%s] Failed to load module!") % p_source_file);
	const Ref<ComputeShaderFile> slang_shader = module->compile_shader(p_options.get("entry_points", {}));
	ERR_FAIL_NULL_V_MSG(slang_shader, ERR_COMPILATION_FAILED, String("[%s] Failed to compile shader!") % module->get_file_path());
	const String base_error = slang_shader->get_base_error();
	if (!base_error.is_empty()) {
		UtilityFunctions::push_error(String("[%s] %s") % Array { p_source_file, base_error });
	}

	for (const Ref<ComputeShaderKernel> kernel : slang_shader->get_kernels()) {
		const String compile_error = kernel->get_compile_error().trim_suffix("\n");
		if (!compile_error.is_empty()) {
			UtilityFunctions::push_error(String("[%s] Slang compile error:\n%s") % Array({ module->get_file_path(), compile_error }));
		}
	}

	const String out_filename = p_save_path + String(".") + _get_save_extension();
	return ResourceSaver::get_singleton()->save(slang_shader, out_filename);
}
