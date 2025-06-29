@tool
extends EditorPlugin

var _slang_importer: SlangShaderImporter

func _enter_tree() -> void:
	_slang_importer = SlangShaderImporter.new()
	add_import_plugin(_slang_importer)

func _exit_tree() -> void:
	remove_import_plugin(_slang_importer)
	_slang_importer = null
