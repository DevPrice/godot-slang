#pragma once

#include "slang.h"

#include <compute_shader_kernel.h>
#include <godot_cpp/classes/editor_import_plugin.hpp>

#include "compute_shader_shape.h"

using namespace godot;

class SlangReflectionContext {

public:

	explicit SlangReflectionContext(slang::ProgramLayout* program_layout) : program_layout(program_layout) {}

	Dictionary get_param_reflection(slang::IMetadata* metadata) const;
	[[nodiscard]] TypedArray<Dictionary> get_buffers_reflection() const;

	template<typename T>
	Dictionary get_attributes(T* reflection) const {
		ERR_FAIL_NULL_V(reflection, {});
		Dictionary param_attributes{};
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

	template<typename T>
	static StringName get_name(T* reflection, const Dictionary& attributes) {
		ERR_FAIL_NULL_V(reflection, {});
		if (attributes.has("gd_Name")) {
			const Dictionary& name_attribute = attributes["gd_Name"];
			return name_attribute["name"];
		}
		return reflection->getName();
	}

private:
	slang::ProgramLayout* program_layout;

	Ref<ShaderTypeLayoutShape> _get_shape(slang::TypeLayoutReflection* type_layout, bool include_property_info = true) const;
	bool _is_autobind(slang::VariableReflection* var) const;
	slang::TypeReflection* _get_attribute_type(slang::Attribute* attribute) const;
	String _get_attribute_argument_name(slang::Attribute* attribute, unsigned int argument_index) const;
	bool _get_godot_type(slang::TypeReflection* type, const Dictionary& attributes, Variant::Type& out_type, PropertyHint& out_hint, String& out_hint_string) const;
	bool _get_godot_array_type(slang::TypeReflection* type, const Dictionary& attributes, Variant::Type& out_type, PropertyHint& out_hint, String& out_hint_string) const;
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

private:
	static SlangResult _create_session(slang::ISession** out_session, const Dictionary& options, bool enable_glsl = false);
	static Error _slang_compile_kernels(slang::IModule* slang_module, TypedArray<ComputeShaderKernel>& out_kernels);
	static Ref<ComputeShaderKernel> _slang_compile_kernel(slang::ISession* session, slang::IModule* slang_module, slang::IEntryPoint* entry_point);
	static slang::IGlobalSession* _get_global_session(bool enable_glsl = false);
};
