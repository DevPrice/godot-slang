#include "slang_shader_editor_plugin.h"

#include "godot_cpp/classes/project_settings.hpp"

using namespace godot;

void SlangShaderEditorPlugin::_bind_methods() {
}

String SlangShaderEditorPlugin::get_modules_path() {
	const String extension_path = ProjectSettings::get_singleton()->globalize_path("uid://blqvpxodges3r");
	return extension_path.get_base_dir().path_join("modules");
}

PackedStringArray SlangShaderEditorPlugin::get_search_paths() {
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

void SlangShaderEditorPlugin::_register_project_settings() {
	_register_project_setting("slang/importer/search_paths", Variant::ARRAY, PROPERTY_HINT_ARRAY_TYPE, String("%d/%d:") % Array { Variant::STRING, PROPERTY_HINT_DIR }, Array());
}

void SlangShaderEditorPlugin::_register_project_setting(const String& name, const Variant::Type type, const PropertyHint hint, const String& hint_string, const Variant& default_value) {
	ProjectSettings* project_settings = ProjectSettings::get_singleton();
	ERR_FAIL_NULL(project_settings);

	if (!project_settings->has_setting(name)) {
		project_settings->set_setting(name, default_value);
	}

	project_settings->set_initial_value(name, default_value);

	Dictionary setting_info{};
	setting_info["name"] = name;
	setting_info["type"] = type;
	setting_info["hint"] = hint;
	setting_info["hint_string"] = hint_string;
	project_settings->add_property_info(setting_info);
}

void SlangShaderEditorPlugin::_enter_tree() {
	EditorPlugin::_enter_tree();
	import_plugin = Ref(memnew(SlangShaderImporter));
	add_import_plugin(import_plugin);
	_register_project_settings();
}

void SlangShaderEditorPlugin::_exit_tree() {
	EditorPlugin::_exit_tree();
	if (import_plugin.is_valid()) {
		remove_import_plugin(import_plugin);
		import_plugin.unref();
	}
}
