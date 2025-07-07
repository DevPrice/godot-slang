#pragma once

#include "godot_cpp/classes/editor_plugin.hpp"

#include "slang_shader_importer.h"

using namespace godot;

class SlangShaderEditorPlugin : public EditorPlugin {
	GDCLASS(SlangShaderEditorPlugin, EditorPlugin)

protected:
	static void _bind_methods();

private:
	Ref<SlangShaderImporter> import_plugin;

public:
	void _enter_tree() override;
	void _exit_tree() override;
};
