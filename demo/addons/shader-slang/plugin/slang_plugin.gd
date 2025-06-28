@tool
extends EditorPlugin

var _slang_importer: SlangShaderImporter

func _enter_tree() -> void:
	_slang_importer = SlangShaderImporter.new()
	add_import_plugin(_slang_importer)
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
