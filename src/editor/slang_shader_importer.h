#pragma once

#include "slang.h"

#include <compute_shader_kernel.h>
#include <godot_cpp/classes/editor_import_plugin.hpp>
#include <optional>

#include "attributes.h"
#include "compute_shader_cursor.h"
#include "compute_shader_shape.h"

struct ShapeOptions {
	int64_t implicit_offset = 0;
	int64_t slot_offset = 0;
	bool include_property_info = true;
	bool include_bindings = false;
	slang::BindingType implicit_buffer_type = slang::BindingType::ConstantBuffer;
};

class SlangReflectionContext {

public:
	explicit SlangReflectionContext(slang::ProgramLayout* program_layout) : program_layout(program_layout) {}

	[[nodiscard]] godot::Ref<StructTypeLayoutShape> get_params_shape() const;
	[[nodiscard]] godot::Ref<StructTypeLayoutShape> get_entry_point_params_shape(slang::EntryPointReflection* entry_point_reflection) const;
	[[nodiscard]] godot::Variant to_json() const;

	template<typename T>
	godot::Dictionary get_attributes(T* reflection) const {
		ERR_FAIL_NULL_V(reflection, {});
		godot::Dictionary param_attributes{};
		for (size_t attribute_index = 0; attribute_index < reflection->getUserAttributeCount(); ++attribute_index) {
			if (slang::Attribute* attribute = reflection->getUserAttributeByIndex(attribute_index)) {
				godot::Dictionary arguments{};
				for (size_t argument_index = 0; argument_index < attribute->getArgumentCount(); ++argument_index) {
					godot::String argument_name = _get_attribute_argument_name(attribute, argument_index);
					arguments.set(argument_name, _to_godot_value(attribute, argument_index));
				}
				param_attributes.set(attribute->getName(), arguments);
			}
		}
		return param_attributes;
	}

	static godot::Variant get_default_value(slang::VariableReflection* var);

	template<typename T>
	static godot::StringName get_name(T* reflection, const godot::Dictionary& attributes) {
		ERR_FAIL_NULL_V(reflection, {});
		if (attributes.has(GodotAttributes::name())) {
			const godot::Dictionary& name_attribute = attributes[GodotAttributes::name()];
			return name_attribute["name"];
		}
		return reflection->getName();
	}

private:
	slang::ProgramLayout* program_layout;

	godot::Ref<ShaderTypeLayoutShape> _get_shape(slang::TypeLayoutReflection* type_layout, const ShapeOptions& shape_options = ShapeOptions{}) const;
	slang::TypeReflection* _get_attribute_type(slang::Attribute* attribute) const;
	godot::String _get_attribute_argument_name(slang::Attribute* attribute, unsigned int argument_index) const;
	bool _get_godot_type(slang::TypeReflection* type, const godot::Dictionary& attributes, godot::Variant::Type& out_type, godot::PropertyHint& out_hint, godot::String& out_hint_string) const;
	bool _get_godot_array_type(slang::TypeReflection* type, const godot::Dictionary& attributes, godot::Variant::Type& out_type, godot::PropertyHint& out_hint, godot::String& out_hint_string) const;
	godot::Variant _to_godot_value(slang::Attribute* attribute, uint32_t argument_index) const;

	static std::optional<godot::RenderingDevice::UniformType> _to_godot_uniform_type(slang::BindingType type);
};

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
	static SlangResult _create_session(slang::ISession** out_session, const godot::Dictionary& options, bool enable_glsl = false);
	static godot::Error _slang_compile_kernels(slang::IModule* slang_module, godot::TypedArray<ComputeShaderKernel>& out_kernels, const godot::PackedStringArray& additional_entry_points, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape);
	static godot::Ref<ComputeShaderKernel> _slang_compile_kernel(slang::ISession* session, slang::IModule* slang_module, slang::IEntryPoint* entry_point, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape);
	static void _get_used_bindings_sets(slang::IMetadata* metadata, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape, const godot::Ref<ShaderTypeLayoutShape>& entry_point_params_shape, godot::Dictionary& out_used_binding_sets, int64_t kernel_space_offset, int64_t kernel_slot_offset);
	static bool _is_location_used(slang::IMetadata* metadata, int64_t space, int64_t binding_index);
	static slang::IGlobalSession* _get_global_session(bool enable_glsl = false);
};
