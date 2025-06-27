#pragma once

#include <godot_cpp/classes/editor_import_plugin.hpp>

using namespace godot;

class SlangShaderImporter final : public EditorImportPlugin {
	GDCLASS(SlangShaderImporter, EditorImportPlugin)

protected:
	static void _bind_methods();

public:
	[[nodiscard]] String _get_importer_name() const override;
	[[nodiscard]] String _get_visible_name() const override;
	[[nodiscard]] int32_t _get_preset_count() const override;
	[[nodiscard]] String _get_preset_name(int32_t p_preset_index) const override;
	[[nodiscard]] PackedStringArray _get_recognized_extensions() const override;
	[[nodiscard]] TypedArray<Dictionary> _get_import_options(const String &p_path, int32_t p_preset_index) const override;
	[[nodiscard]] String _get_save_extension() const override;
	[[nodiscard]] String _get_resource_type() const override;
	[[nodiscard]] float _get_priority() const override;
	[[nodiscard]] int32_t _get_import_order() const override;
	[[nodiscard]] bool _get_option_visibility(const String &p_path, const StringName &p_option_name, const Dictionary &p_options) const override;
	[[nodiscard]] Error _import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options, const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const override;

	static String get_editor_setting_slangc_location();
	static String get_editor_setting_gen_path();
};
