#pragma once

#include <godot_cpp/classes/editor_import_plugin.hpp>

// Imports a `.slang` file declaring a [gd::Material] entry point as a plain [Shader], so it can
// be assigned to a ShaderMaterial directly. A file has to be imported as either a material or a
// SlangShaderFile, never both, so this is offered alongside SlangShaderImporter under "Import As"
// rather than being folded into it.
class SlangMaterialImporter final : public godot::EditorImportPlugin {
	GDCLASS(SlangMaterialImporter, EditorImportPlugin)

protected:
	static void _bind_methods();

public:
	[[nodiscard]] godot::String _get_importer_name() const override;
	[[nodiscard]] godot::String _get_visible_name() const override;
	[[nodiscard]] int32_t _get_format_version() const override;
	[[nodiscard]] int32_t _get_preset_count() const override;
	[[nodiscard]] godot::String _get_preset_name(int32_t p_preset_index) const override;
	[[nodiscard]] godot::PackedStringArray _get_recognized_extensions() const override;
	[[nodiscard]] godot::TypedArray<godot::Dictionary> _get_import_options(const godot::String& p_path, int32_t p_preset_index) const override;
	[[nodiscard]] godot::String _get_save_extension() const override;
	[[nodiscard]] godot::String _get_resource_type() const override;
	[[nodiscard]] float _get_priority() const override;
	[[nodiscard]] int32_t _get_import_order() const override;
	[[nodiscard]] bool _get_option_visibility(const godot::String& p_path, const godot::StringName& p_option_name, const godot::Dictionary& p_options) const override;
	[[nodiscard]] godot::Error _import(const godot::String& p_source_file, const godot::String& p_save_path, const godot::Dictionary& p_options, const godot::TypedArray<godot::String>& p_platform_variants, const godot::TypedArray<godot::String>& p_gen_files) const override;
};
