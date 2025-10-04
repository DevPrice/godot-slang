#pragma once

#include "slang.h"

#include <compute_shader_kernel.h>
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
	[[nodiscard]] TypedArray<Dictionary> _get_import_options(const String& p_path, int32_t p_preset_index) const override;
	[[nodiscard]] String _get_save_extension() const override;
	[[nodiscard]] String _get_resource_type() const override;
	[[nodiscard]] float _get_priority() const override;
	[[nodiscard]] int32_t _get_import_order() const override;
	[[nodiscard]] bool _get_option_visibility(const String& p_path, const StringName& p_option_name, const Dictionary& p_options) const override;
	[[nodiscard]] Error _import(const String& p_source_file, const String& p_save_path, const Dictionary& p_options, const TypedArray<String>& p_platform_variants, const TypedArray<String>& p_gen_files) const override;

private:
	static SlangResult _create_session(slang::ISession** out_session);
	static Error _slang_compile_kernels(slang::IModule* slang_module, TypedArray<ComputeShaderKernel>& out_kernels);
	static Ref<ComputeShaderKernel> _slang_compile_kernel(slang::ISession* session, slang::IModule* slang_module, slang::IEntryPoint* entry_point);
	static Dictionary _get_param_reflection(slang::ProgramLayout* program_layout, slang::IMetadata* metadata);
	static bool _is_autobind(slang::ProgramLayout* program_layout, slang::VariableReflection* var);
	static Dictionary _get_attributes(slang::ProgramLayout* program_layout, slang::VariableReflection* variable_reflection);
	static TypedArray<Dictionary> _get_buffers_reflection(slang::ProgramLayout* program_layout);
	static slang::TypeReflection* _get_attribute_type(slang::Attribute* attribute, slang::ProgramLayout* layout);
	static String _get_attribute_argument_name(slang::Attribute* attribute, unsigned int argument_index, slang::ProgramLayout* layout);
	static bool _get_godot_type(slang::TypeReflection* type, const Dictionary& attributes, Variant::Type& out_type, PropertyHint& out_hint, String& out_hint_string);
	static Variant _to_godot_value(slang::Attribute* attribute, uint32_t argument_index);
	static RenderingDevice::UniformType _to_godot_uniform_type(slang::BindingType type);
	static slang::IGlobalSession* _get_global_session();
};
