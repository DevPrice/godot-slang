#pragma once

#include "slang.h"

#include <compute_shader_kernel.h>
#include <godot_cpp/classes/editor_import_plugin.hpp>

using namespace godot;

class SlangReflectionContext {

public:

	explicit SlangReflectionContext(slang::ProgramLayout* program_layout) : program_layout(program_layout) {}

	Dictionary get_param_reflection(slang::IMetadata* metadata);
	[[nodiscard]] TypedArray<Dictionary> get_buffers_reflection() const;

	template<typename T>
	Dictionary get_attributes(T* reflection) const {
		Dictionary param_attributes{};
		if (!reflection) return param_attributes;
		for (size_t attribute_index = 0; attribute_index < reflection->getUserAttributeCount(); ++attribute_index) {
			if (slang::Attribute* attribute = reflection->getUserAttributeByIndex(attribute_index)) {
				Dictionary arguments{};
				for (size_t argument_index = 0; argument_index < attribute->getArgumentCount(); ++argument_index) {
					String argument_name = _get_attribute_argument_name(attribute, argument_index);
					arguments.set(argument_name, _to_godot_value(attribute, argument_index));
				}
				param_attributes.set(attribute->getName(), arguments);
			}
		}
		return param_attributes;
	}

private:
	slang::ProgramLayout* program_layout;

	Dictionary _get_shape(slang::TypeLayoutReflection* type_layout) const;
	bool _is_autobind(slang::VariableReflection* var) const;
	slang::TypeReflection* _get_attribute_type(slang::Attribute* attribute) const;
	String _get_attribute_argument_name(slang::Attribute* attribute, unsigned int argument_index) const;
	bool _get_godot_type(slang::TypeReflection* type, const Dictionary& attributes, Variant::Type& out_type, PropertyHint& out_hint, String& out_hint_string) const;
	Variant _to_godot_value(slang::Attribute* attribute, uint32_t argument_index) const;

	static RenderingDevice::UniformType _to_godot_uniform_type(slang::BindingType type);
};

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

	enum MatrixLayout {
		ROW_MAJOR = SLANG_MATRIX_LAYOUT_ROW_MAJOR,
		COLUMN_MAJOR = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
	};

private:
	static SlangResult _create_session(slang::ISession** out_session, const Dictionary& options, bool enable_glsl = false);
	static Error _slang_compile_kernels(slang::IModule* slang_module, TypedArray<ComputeShaderKernel>& out_kernels);
	static Ref<ComputeShaderKernel> _slang_compile_kernel(slang::ISession* session, slang::IModule* slang_module, slang::IEntryPoint* entry_point);
	static slang::IGlobalSession* _get_global_session(bool enable_glsl = false);
};

VARIANT_ENUM_CAST(SlangShaderImporter::MatrixLayout)
