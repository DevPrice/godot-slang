#pragma once

#include "godot_cpp/classes/editor_plugin.hpp"

#include "slang_shader_importer.h"

class SlangShaderEditorPlugin : public godot::EditorPlugin {
	GDCLASS(SlangShaderEditorPlugin, EditorPlugin)

protected:
	static void _bind_methods();

public:
	static godot::String get_modules_path();
	static godot::Dictionary get_preprocessor_macros();
	static godot::PackedStringArray get_search_paths();

private:
	godot::Ref<SlangShaderImporter> import_plugin;

	static void _register_project_settings();
	static void _register_project_setting(const godot::String& name, godot::Variant::Type type, godot::PropertyHint hint = godot::PROPERTY_HINT_NONE, const godot::String& hint_string = "", const godot::Variant& default_value = godot::Variant{});

public:
	void _enter_tree() override;
	void _exit_tree() override;
};
