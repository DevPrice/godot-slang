#pragma once

#include "slang.h"

#include "godot_cpp/classes/image_texture_layered.hpp"

#include "attributes.h"
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

	static int64_t _get_scalar_size(slang::TypeReflection::ScalarType scalar_type);
	static std::optional<godot::RenderingDevice::UniformType> _to_godot_uniform_type(slang::BindingType type, SlangResourceShape resource_shape);
};
