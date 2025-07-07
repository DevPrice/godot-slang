#include "slang_shader_editor_plugin.h"

void SlangShaderEditorPlugin::_bind_methods() {
}

void SlangShaderEditorPlugin::_enter_tree() {
	EditorPlugin::_enter_tree();
	import_plugin = Ref(memnew(SlangShaderImporter));
	add_import_plugin(import_plugin);
}

void SlangShaderEditorPlugin::_exit_tree() {
	EditorPlugin::_exit_tree();
	if (import_plugin.is_valid()) {
		remove_import_plugin(import_plugin);
		import_plugin.unref();
	}
}
