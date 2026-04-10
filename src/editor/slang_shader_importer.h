#pragma once

#include "slang.h"

#include <compute_shader_kernel.h>
#include <godot_cpp/classes/editor_import_plugin.hpp>
#include <optional>

#include "attributes.h"
#include "compute_shader_cursor.h"
#include "compute_shader_shape.h"

class SlangShaderImporter final : public godot::EditorImportPlugin {
	GDCLASS(SlangShaderImporter, EditorImportPlugin)

protected:
	static void _bind_methods();

public:
	[[nodiscard]] godot::String _get_importer_name() const override;
	[[nodiscard]] godot::String _get_visible_name() const override;
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

private:
	static godot::Error _slang_compile_kernels(slang::IModule* slang_module, godot::TypedArray<ComputeShaderKernel>& out_kernels, const godot::PackedStringArray& additional_entry_points, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape);
	static godot::Ref<ComputeShaderKernel> _slang_compile_kernel(slang::ISession* session, slang::IModule* slang_module, slang::IEntryPoint* entry_point, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape);
	static void _get_used_bindings_sets(slang::IMetadata* metadata, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape, const godot::Ref<ShaderTypeLayoutShape>& entry_point_params_shape, godot::Dictionary& out_used_binding_sets, int64_t kernel_space_offset, int64_t kernel_slot_offset);
	static bool _is_location_used(slang::IMetadata* metadata, int64_t space, int64_t binding_index);
};
