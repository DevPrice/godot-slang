@tool
extends EditorPlugin

var _slang_importer: SlangShaderImporter

func _enter_tree() -> void:
	_slang_importer = SlangShaderImporter.new()
	add_import_plugin(_slang_importer)
	var editor_settings := EditorInterface.get_editor_settings()
	if not editor_settings.has_setting(SlangShaderImporter.get_editor_setting_slangc_location()):
		editor_settings.set_setting(SlangShaderImporter.get_editor_setting_slangc_location(), "")
	editor_settings.set_initial_value(SlangShaderImporter.get_editor_setting_slangc_location(), "", false)
	editor_settings.add_property_info({
		"name": SlangShaderImporter.get_editor_setting_slangc_location(),
		"type": TYPE_STRING,
		"hint": PROPERTY_HINT_FILE,
		"hint_string": "*.exe",
	})
	if not ProjectSettings.has_setting(SlangShaderImporter.get_editor_setting_gen_path()):
		ProjectSettings.set_setting(SlangShaderImporter.get_editor_setting_gen_path(), "res://gen")
	ProjectSettings.set_initial_value(SlangShaderImporter.get_editor_setting_gen_path(), "res://gen")
	ProjectSettings.add_property_info({
		"name": SlangShaderImporter.get_editor_setting_gen_path(),
		"type": TYPE_STRING,
		"hint": PROPERTY_HINT_GLOBAL_DIR,
	})

func _exit_tree() -> void:
	remove_import_plugin(_slang_importer)
	_slang_importer = null
