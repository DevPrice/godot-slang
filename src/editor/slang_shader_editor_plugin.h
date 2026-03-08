#pragma once

#include "godot_cpp/classes/editor_plugin.hpp"

#include "slang_shader_importer.h"

class SlangShaderEditorPlugin : public godot::EditorPlugin {
	GDCLASS(SlangShaderEditorPlugin, EditorPlugin)

protected:
	static void _bind_methods();

private:
	godot::Ref<SlangShaderImporter> import_plugin;

public:
	void _enter_tree() override;
	void _exit_tree() override;
};
