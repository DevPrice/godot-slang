#include <optional>

#include "slang-com-ptr.h"

#include "godot_cpp/classes/cubemap.hpp"
#include "godot_cpp/classes/cubemap_array.hpp"
#include "godot_cpp/classes/image_texture_layered.hpp"
#include "godot_cpp/classes/json.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/texture2d.hpp"
#include "godot_cpp/classes/texture3d.hpp"

#include "attributes.h"
#include "compute_shader_shape.h"
#include "enums.h"
#include "slang_shader_importer.h"

#include "reflection_context.h"

#include "slang_blob.h"

using namespace godot;

Ref<StructTypeLayoutShape> SlangReflectionContext::get_params_shape() const {
	ERR_FAIL_NULL_V(program_layout, {});
	return _get_shape(program_layout->getGlobalParamsTypeLayout(), { .include_bindings = true });
}

Ref<StructTypeLayoutShape> SlangReflectionContext::get_entry_point_params_shape(slang::EntryPointReflection* entry_point_reflection) const {
	ERR_FAIL_NULL_V(entry_point_reflection, {});
	return _get_shape(entry_point_reflection->getTypeLayout(), {
		.implicit_offset = entry_point_reflection->getTypeLayout()->getSize() > 0,
		.include_bindings = true,
		.implicit_buffer_type = slang::BindingType::PushConstant
	});
}

Ref<ShaderTypeLayoutShape> SlangReflectionContext::_get_shape(slang::TypeLayoutReflection* type_layout, const ShapeOptions& shape_options) const {
	ERR_FAIL_NULL_V(type_layout, nullptr);

	TypedArray<Dictionary> bindings{};

	if (shape_options.include_bindings) {
		if (shape_options.implicit_offset) {
			BindingRange binding_range{};
			binding_range.size = type_layout->getSize();
			binding_range.alignment = 16;
			binding_range.type = static_cast<ShaderTypeLayoutShape::BindingType>(static_cast<int64_t>(shape_options.implicit_buffer_type));
			if (const auto uniform_type = _to_godot_uniform_type(shape_options.implicit_buffer_type, type_layout->getResourceShape())) {
				binding_range.uniform_type = *uniform_type;
			}
			binding_range.slot_offset = 0;
			binding_range.binding_count = 1;
			bindings.push_back(Dictionary(binding_range));
		}

		for (int i = 0; i < type_layout->getBindingRangeCount(); i++) {
			BindingRange binding_range{};
			const slang::BindingType binding_type = type_layout->getBindingRangeType(i);
			const auto leaf_type = type_layout->getBindingRangeLeafTypeLayout(i);
			const Ref<ShaderTypeLayoutShape> leaf_shape = _get_shape(leaf_type);
			ERR_FAIL_NULL_V(leaf_type, nullptr);
			binding_range.type = static_cast<ShaderTypeLayoutShape::BindingType>(static_cast<int64_t>(binding_type));
			if (const auto uniform_type = _to_godot_uniform_type(binding_type, leaf_type->getResourceShape())) {
				binding_range.uniform_type = *uniform_type;
			}
			if (binding_type == slang::BindingType::PushConstant) {
				binding_range.size = leaf_shape->get_size();
			}
			if (binding_type == slang::BindingType::PushConstant || binding_type == slang::BindingType::ConstantBuffer) {
				binding_range.alignment = 16;
			}
			if (binding_type == slang::BindingType::ConstantBuffer || binding_type == slang::BindingType::ParameterBlock) {
				binding_range.leaf_shape = leaf_shape;
			}

			const int64_t set_index = type_layout->getBindingRangeDescriptorSetIndex(i);
			const int64_t range_index = type_layout->getBindingRangeFirstDescriptorRangeIndex(i);
			binding_range.slot_offset = binding_type == slang::BindingType::ParameterBlock
				? 0 : shape_options.slot_offset + type_layout->getDescriptorSetDescriptorRangeIndexOffset(set_index, range_index);
			binding_range.binding_count = type_layout->getBindingRangeBindingCount(i);
			bindings.push_back(Dictionary(binding_range));
		}
	}

	switch (type_layout->getKind()) {
		case slang::TypeReflection::Kind::Scalar:
		case slang::TypeReflection::Kind::Vector:
		case slang::TypeReflection::Kind::Matrix: {
			Ref<VariantTypeLayoutShape> shape;
			shape.instantiate();
			shape->set_size(static_cast<int64_t>(type_layout->getSize()));
			shape->set_matrix_layout(static_cast<ShaderTypeLayoutShape::MatrixLayout>(type_layout->getMatrixLayoutMode()));
			shape->set_bindings(bindings);
			return shape;
		}
		case slang::TypeReflection::Kind::Struct: {
			Ref<StructTypeLayoutShape> shape;
			shape.instantiate();
			shape->set_size(static_cast<int64_t>(type_layout->getSize()));
			shape->set_alignment(type_layout->getAlignment());
			shape->set_bindings(bindings);
			const StringName type_name(type_layout->getName());
			if (!type_name.is_empty()) {
				shape->set_meta(StringName("type_name"), type_name);
			}

			Dictionary field_shapes{};
			for (int i = 0; i < type_layout->getFieldCount(); i++) {
				slang::VariableLayoutReflection* field = type_layout->getFieldByIndex(i);
				FieldShape field_info{};
				const Dictionary field_attributes = get_attributes(field->getVariable());
				const bool is_exported = field_attributes.has(GodotAttributes::export_property()) || field_attributes.has(GodotAttributes::export_param());
				if (field->getCategory() == slang::ParameterCategory::None) {
					continue;
				}

				const Ref<ShaderTypeLayoutShape> field_shape = _get_shape(field->getTypeLayout(), {
					.implicit_offset = type_layout->getFieldBindingRangeOffset(i) + shape_options.implicit_offset,
					.slot_offset = shape_options.slot_offset,
					.include_property_info = shape_options.include_property_info && is_exported
				});
				const StringName field_name = get_name(field, field_attributes);

				field_info.name = field_name;
				field_info.shape = field_shape;
				field_info.user_attributes = field_attributes;
				field_info.byte_offset = static_cast<int64_t>(field->getOffset());

				// TODO: I feel like I shouldn't need to have this logic, but I'm not sure how to find the uniform buffer binding correctly otherwise.
				slang::TypeLayoutReflection* field_type = field->getTypeLayout();
				const bool has_uniform_data = field_type->getParameterCategory() == slang::ParameterCategory::Uniform || field_type->getParameterCategory() == slang::ParameterCategory::Mixed;
				const bool has_own_binding_ranges = field_type->getKind() == slang::TypeReflection::Kind::ConstantBuffer || field_type->getKind() == slang::TypeReflection::Kind::ParameterBlock;
				if (has_uniform_data && !has_own_binding_ranges) {
					field_info.binding_offset = 0;
				} else {
					field_info.binding_offset = type_layout->getFieldBindingRangeOffset(i) + shape_options.implicit_offset;
				}

				if (shape_options.include_property_info) {
					Variant::Type type;
					PropertyHint hint;
					String hint_string;
					if (_get_godot_type(field->getType(), field_attributes, type, hint, hint_string)) {
						const uint32_t usage = is_exported ? PROPERTY_USAGE_DEFAULT : PROPERTY_USAGE_NONE;
						if (field_attributes.has(GodotAttributes::property_hint())) {
							const Dictionary property_hint_attr = field_attributes[GodotAttributes::property_hint()];
							hint = static_cast<PropertyHint>(static_cast<uint64_t>(property_hint_attr["property_hint"]));
							hint_string = property_hint_attr["hint_string"];
						}
						field_info.property_info = {type, get_name(field, field_attributes), hint, hint_string, usage};
					}

					field_info.default_value = get_default_value(field->getVariable());
				}

				field_shapes.set(field_name, Dictionary(field_info));
			}
			shape->set_properties(field_shapes);
			if (slang::TypeReflection* type = type_layout->getType()) {
				shape->set_user_attributes(get_attributes(type));
			}

			return shape;
		}
		case slang::TypeReflection::Kind::SamplerState:
		case slang::TypeReflection::Kind::Resource: {
			const SlangResourceShapeIntegral base_resource_shape = type_layout->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK;
			// TODO: We probably don't need to know the type in the shape
			const slang::BindingType binding_type = type_layout->getBindingRangeType(0);
			const auto uniform_type = _to_godot_uniform_type(binding_type, type_layout->getResourceShape());
			if (!uniform_type) {
				UtilityFunctions::push_warning("Unknown binding type: ", static_cast<int64_t>(binding_type));
			}
			if (base_resource_shape == SLANG_BYTE_ADDRESS_BUFFER) {
				Ref<ResourceTypeLayoutShape> shape;
				shape.instantiate();
				shape->set_resource_type(ResourceTypeLayoutShape::RAW_BYTES);
				shape->set_uniform_type(uniform_type ? *uniform_type : static_cast<RenderingDevice::UniformType>(-1));
				shape->set_bindings(bindings);
				return shape;
			}
			if (base_resource_shape == SLANG_TEXTURE_BUFFER) {
				Ref<VariantTypeLayoutShape> element_shape;
				auto element_type = type_layout->getResourceResultType();
				ERR_FAIL_NULL_V(element_type, nullptr);
				const int64_t stride = _get_scalar_size(element_type->getScalarType()) * element_type->getColumnCount() * element_type->getRowCount();
				element_shape.instantiate();
				element_shape->set_size(stride);
				element_shape->set_matrix_layout(static_cast<ShaderTypeLayoutShape::MatrixLayout>(type_layout->getMatrixLayoutMode()));
				Ref<ArrayTypeLayoutShape> shape;
				shape.instantiate();
				shape->set_element_shape(element_shape);
				shape->set_bindings(bindings);
				shape->set_stride(stride);
				return shape;
			}
			if (base_resource_shape != SLANG_STRUCTURED_BUFFER) {
				Ref<ResourceTypeLayoutShape> shape;
				shape.instantiate();
				shape->set_resource_type(ResourceTypeLayoutShape::UNKNOWN);
				shape->set_uniform_type(uniform_type ? *uniform_type : static_cast<RenderingDevice::UniformType>(-1));
				shape->set_bindings(bindings);
				return shape;
			}
		}
		case slang::TypeReflection::Kind::Array:
		case slang::TypeReflection::Kind::ShaderStorageBuffer: {
			Ref<ArrayTypeLayoutShape> shape;
			shape.instantiate();
			shape->set_element_shape(
				_get_shape(type_layout->getElementTypeLayout(), {
					.slot_offset = shape_options.slot_offset,
					.include_property_info = shape_options.include_property_info
				})
			);
			if (type_layout->isArray()) {
				shape->set_size(static_cast<int64_t>(type_layout->getSize()));
			}
			shape->set_stride(static_cast<int64_t>(type_layout->getElementTypeLayout()->getStride()));
			shape->set_element_count(type_layout->getElementCount());
			shape->set_bindings(bindings);
			return shape;
		}
		case slang::TypeReflection::Kind::ConstantBuffer:
		case slang::TypeReflection::Kind::ParameterBlock: {
			slang::VariableLayoutReflection* element_var = type_layout->getElementVarLayout();
			slang::TypeLayoutReflection* element_type = element_var->getTypeLayout();
			const Ref<ShaderTypeLayoutShape> element_shape = _get_shape(element_type,{
				.implicit_offset = element_type->getSize() > 0,
				.slot_offset = static_cast<int64_t>(element_var->getOffset(slang::ParameterCategory::DescriptorTableSlot)),
				.include_property_info = shape_options.include_property_info,
				.include_bindings = true,
				.implicit_buffer_type = shape_options.implicit_buffer_type,
			});
			ERR_FAIL_NULL_V(element_shape, nullptr);
			element_shape->set_meta(StringName("container_kind"), static_cast<int64_t>(type_layout->getKind()));
			return element_shape;
		}
		default:
			break;
	}

	return nullptr;
}

// TODO: Surely there is a better way to do this
slang::TypeReflection* SlangReflectionContext::_get_attribute_type(slang::Attribute* attribute) const {
	ERR_FAIL_NULL_V(attribute, nullptr);
	const auto type_name = (String(attribute->getName()) + "Attribute").utf8();
	return program_layout->findTypeByName(type_name.get_data());
}

String SlangReflectionContext::_get_attribute_argument_name(slang::Attribute* attribute, const unsigned int argument_index) const {
	ERR_FAIL_NULL_V(attribute, String("argument") + String::num_int64(argument_index));
	slang::TypeReflection* attribute_type = _get_attribute_type(attribute);
	ERR_FAIL_NULL_V(attribute_type, String("argument") + String::num_int64(argument_index));
	slang::VariableReflection* field = attribute_type->getFieldByIndex(argument_index);
	return get_name(field, get_attributes(field));
}

 bool SlangReflectionContext::_get_godot_type(slang::TypeReflection* type, const Dictionary& attributes, Variant::Type& out_type, PropertyHint& out_hint, String& out_hint_string) const {
	ERR_FAIL_NULL_V(type, false);
	out_hint = PROPERTY_HINT_NONE;
	out_hint_string = "";
	switch (type->getKind()) {
		case slang::TypeReflection::Kind::Scalar:
			switch (type->getScalarType()) {
				case slang::TypeReflection::ScalarType::Bool:
					out_type = Variant::BOOL;
					return true;
				case slang::TypeReflection::ScalarType::Float16:
				case slang::TypeReflection::ScalarType::Float32:
				case slang::TypeReflection::ScalarType::Float64:
					out_type = Variant::FLOAT;
					return true;
				case slang::TypeReflection::ScalarType::Int32:
				case slang::TypeReflection::ScalarType::UInt32:
				case slang::TypeReflection::ScalarType::Int64:
				case slang::TypeReflection::ScalarType::UInt64:
				case slang::TypeReflection::ScalarType::Int8:
				case slang::TypeReflection::ScalarType::UInt8:
				case slang::TypeReflection::ScalarType::Int16:
				case slang::TypeReflection::ScalarType::UInt16:
					out_type = Variant::INT;
					return true;
				default:
					break;
			}
		case slang::TypeReflection::Kind::Enum: {
			out_type = Variant::INT;
			Dictionary enum_values{};
			out_hint = PROPERTY_HINT_ENUM;
			for (uint32_t i = 0; i < type->getFieldCount(); i++) {
				slang::VariableReflection* field = type->getFieldByIndex(i);
				const String enum_name = get_name(field, get_attributes(field));
				int64_t enum_value = 0;
				field->getDefaultValueInt(&enum_value);
				enum_values.set(enum_name.capitalize(), enum_value);
			}
			out_hint_string = Enums::get_enum_hint_string(enum_values);
			return true;
		}
		case slang::TypeReflection::Kind::Vector: {
			const bool is_color = attributes.has(GodotAttributes::color());
			switch (type->getColumnCount()) {
				case 2:
					out_type = Variant::VECTOR2;
					return true;
				case 3:
					if (is_color) {
						out_type = Variant::COLOR;
						out_hint = PROPERTY_HINT_COLOR_NO_ALPHA;
					} else {
						out_type = Variant::VECTOR3;
					}
					return true;
				case 4:
					out_type = is_color ? Variant::COLOR : Variant::VECTOR4;
					return true;
				default: break;
			}
			break;
		}
		case slang::TypeReflection::Kind::Array:
			return _get_godot_array_type(type->getElementType(), attributes, out_type, out_hint, out_hint_string);
		case slang::TypeReflection::Kind::Resource: {
			const SlangResourceShape resource_shape = type->getResourceShape();
			const unsigned base_shape = resource_shape & SLANG_RESOURCE_BASE_SHAPE_MASK;
			if (resource_shape & SLANG_TEXTURE_ARRAY_FLAG) {
				out_type = Variant::OBJECT;
				out_hint = PROPERTY_HINT_RESOURCE_TYPE;
				out_hint_string = base_shape == SLANG_TEXTURE_CUBE
					? CubemapArray::get_class_static()
					: ImageTextureLayered::get_class_static();
				return true;
			}
			switch (base_shape) {
				case SLANG_TEXTURE_2D:
					out_type = Variant::OBJECT;
					out_hint = PROPERTY_HINT_RESOURCE_TYPE;
					out_hint_string = Texture2D::get_class_static();
					return true;
				case SLANG_TEXTURE_3D:
					out_type = Variant::OBJECT;
					out_hint = PROPERTY_HINT_RESOURCE_TYPE;
					out_hint_string = Texture3D::get_class_static();
					return true;
				case SLANG_TEXTURE_CUBE:
					out_type = Variant::OBJECT;
					out_hint = PROPERTY_HINT_RESOURCE_TYPE;
					out_hint_string = Cubemap::get_class_static();
					return true;
				case SLANG_BYTE_ADDRESS_BUFFER:
					out_type = Variant::PACKED_BYTE_ARRAY;
					return true;
				case SLANG_TEXTURE_BUFFER:
					return _get_godot_array_type(type->getResourceResultType(), attributes, out_type, out_hint, out_hint_string);
				case SLANG_STRUCTURED_BUFFER:
					return _get_godot_array_type(type->getElementType(), attributes, out_type, out_hint, out_hint_string);
				default:
					break;
			}
			break;
		}
		case slang::TypeReflection::Kind::ParameterBlock:
		case slang::TypeReflection::Kind::ConstantBuffer: {
			if (_get_godot_type(type->getElementType(), attributes, out_type, out_hint, out_hint_string)) {
				return true;
			}
			return false;
		}
		case slang::TypeReflection::Kind::Struct: {
			if (String(type->getName()) == "String") {
				// TODO: Is there a better way to detect the String type?
				out_type = Variant::STRING;
				return true;
			}
			const Dictionary type_attributes = get_attributes(type);
			if (type_attributes.has(GodotAttributes::type())) {
				const Dictionary type_attr = type_attributes[GodotAttributes::type()];
				out_type = static_cast<Variant::Type>(static_cast<int64_t>(type_attr["type"]));
				return true;
			}
			const Dictionary class_args = type_attributes.get(GodotAttributes::class_name(), Dictionary());
			const StringName class_name = class_args.get("class_name", StringName());
			if (!class_name.is_empty()) {
				if (ClassDB::is_parent_class(class_name, Resource::get_class_static())) {
					out_type = Variant::OBJECT;
					out_hint = PROPERTY_HINT_RESOURCE_TYPE;
					out_hint_string = class_name;
					return true;
				}
				if (ClassDB::is_parent_class(class_name, Node::get_class_static())) {
					out_type = Variant::OBJECT;
					out_hint = PROPERTY_HINT_NODE_TYPE;
					out_hint_string = class_name;
					return true;
				}
				UtilityFunctions::push_warning("Exported class should be a Resource or Node type: ", class_name);
			}
			return false;
		}
		case slang::TypeReflection::Kind::SamplerState:
			// Not currently supported
			return false;
		case slang::TypeReflection::Kind::Matrix: {
			const int64_t rows = type->getRowCount();
			const int64_t columns = type->getColumnCount();
			if (rows == 4 && columns == 4) {
				out_type = Variant::PROJECTION;
				return true;
			}
			if (rows == 3 && columns == 3) {
				out_type = Variant::BASIS;
				return true;
			}
			if (rows <= 3 && columns <= 3) {
				out_type = Variant::TRANSFORM2D;
				return true;
			}
			if (rows <= 4 && columns <= 4) {
				out_type = Variant::TRANSFORM3D;
				return true;
			}
			// this matrix size doesn't map to a Godot type
			// TODO: maybe expose this as a PackedFloatArray
			return false;
		}
		default:
			break;
	}
	// TODO: Support the other types
	UtilityFunctions::push_warning("Slang: Unknown Godot type: ", type->getName(), String(" (%s)") % static_cast<int64_t>(type->getKind()));
	return false;
}

bool SlangReflectionContext::_get_godot_array_type(slang::TypeReflection* type, const Dictionary& attributes, Variant::Type& out_type, PropertyHint& out_hint, String& out_hint_string) const {
	ERR_FAIL_NULL_V(type, false);

	Variant::Type element_type;
	PropertyHint element_hint;
	String element_hint_string;

	if (_get_godot_type(type, attributes, element_type, element_hint, element_hint_string)) {
		switch (element_type) {
			case Variant::INT:
				if (type->getScalarType() == slang::TypeReflection::ScalarType::Int64) {
					out_type = Variant::PACKED_INT64_ARRAY;
				} else {
					out_type = Variant::PACKED_INT32_ARRAY;
				}
				break;
			case Variant::FLOAT:
				if (type->getScalarType() == slang::TypeReflection::ScalarType::Float64) {
					out_type = Variant::PACKED_FLOAT64_ARRAY;
				} else {
					out_type = Variant::PACKED_FLOAT32_ARRAY;
				}
				break;
			case Variant::VECTOR2:
				out_type = Variant::PACKED_VECTOR2_ARRAY;
				break;
			case Variant::VECTOR3:
				out_type = Variant::PACKED_VECTOR3_ARRAY;
				break;
			case Variant::VECTOR4:
				out_type = Variant::PACKED_VECTOR4_ARRAY;
				break;
			case Variant::COLOR:
				out_type = Variant::PACKED_COLOR_ARRAY;
				break;
			default:
				out_type = Variant::ARRAY;
				out_hint = PROPERTY_HINT_ARRAY_TYPE;
				if (element_type == Variant::Type::OBJECT) {
					out_hint_string = String("%s/%s:%s") % Array { element_type, PROPERTY_HINT_RESOURCE_TYPE, element_hint_string };
				} else {
					out_hint_string = Variant::get_type_name(element_type);
				}
				break;
		}
		return true;
	}
	return false;
}

Variant SlangReflectionContext::_to_godot_value(slang::Attribute* attribute, const uint32_t argument_index) const {
	if (slang::TypeReflection* type_reflection = attribute->getArgumentType(argument_index)) {
		switch (type_reflection->getKind()) {
			case slang::TypeReflection::Kind::Scalar: {
				switch (type_reflection->getScalarType()) {
					case slang::TypeReflection::ScalarType::Bool: {
						int value{};
						if (attribute->getArgumentValueInt(argument_index, &value) == SLANG_OK) {
							return static_cast<bool>(value);
						}
						break;
					}
					case slang::TypeReflection::ScalarType::Int8:
					case slang::TypeReflection::ScalarType::Int16:
					case slang::TypeReflection::ScalarType::Int32:
					case slang::TypeReflection::ScalarType::Int64:
					case slang::TypeReflection::ScalarType::UInt8:
					case slang::TypeReflection::ScalarType::UInt16:
					case slang::TypeReflection::ScalarType::UInt32:
					case slang::TypeReflection::ScalarType::UInt64: {
						int value{};
						if (attribute->getArgumentValueInt(argument_index, &value) == SLANG_OK) {
							return value;
						}
						break;
					}
					case slang::TypeReflection::ScalarType::Float16:
					case slang::TypeReflection::ScalarType::Float32:
					case slang::TypeReflection::ScalarType::Float64: {
						float value{};
						if (attribute->getArgumentValueFloat(argument_index, &value) == SLANG_OK) {
							return value;
						}
						break;
					}
					default:
						break;
				}
			}
			case slang::TypeReflection::Kind::Struct: {
				if (String(type_reflection->getName()) == "String") {
					size_t size{};
					if (const char* value = attribute->getArgumentValueString(argument_index, &size)) {
						return String::utf8(value, size);
					}
				}
				break;
			}
			default:
				break;
		}
		// TODO: Support the other types
		UtilityFunctions::push_warning("Slang: Failed to make Godot value for attribute: ", type_reflection->getName(), String(" (%s)") % static_cast<int64_t>(type_reflection->getKind()));
	} else {
		int value{};
		if (SLANG_SUCCEEDED(attribute->getArgumentValueInt(argument_index, &value))) {
			// this sometimes happens for enum values for some reason
			return value;
		}
		UtilityFunctions::push_error("Slang: Got null type pointer for attribute ", attribute->getName());
	}
	return Variant{};
}

int64_t SlangReflectionContext::_get_scalar_size(const slang::TypeReflection::ScalarType scalar_type) {
	switch (scalar_type) {
		case slang::TypeReflection::ScalarType::Float16:
		case slang::TypeReflection::ScalarType::Int16:
		case slang::TypeReflection::ScalarType::UInt16:
			return 2;
		case slang::TypeReflection::ScalarType::Float64:
		case slang::TypeReflection::ScalarType::Int64:
		case slang::TypeReflection::ScalarType::UInt64:
			return 8;
		default:
			return 4;
	}
}

Variant SlangReflectionContext::get_default_value(slang::VariableReflection* var) {
	if (var->hasDefaultValue()) {
		if (slang::TypeReflection* type = var->getType()) {
			if (type->getKind() == slang::TypeReflection::Kind::Scalar) {
				switch (type->getScalarType()) {
					case slang::TypeReflection::ScalarType::Int8:
					case slang::TypeReflection::ScalarType::Int16:
					case slang::TypeReflection::ScalarType::Int32:
					case slang::TypeReflection::ScalarType::Int64:
					case slang::TypeReflection::ScalarType::UInt8:
					case slang::TypeReflection::ScalarType::UInt16:
					case slang::TypeReflection::ScalarType::UInt32:
					case slang::TypeReflection::ScalarType::UInt64: {
						if (int64_t default_value_int; SLANG_SUCCEEDED(var->getDefaultValueInt(&default_value_int))) {
							return default_value_int;
						}
						break;
					}
					case slang::TypeReflection::ScalarType::Float16:
					case slang::TypeReflection::ScalarType::Float32:
					case slang::TypeReflection::ScalarType::Float64: {
						if (float default_value_float; SLANG_SUCCEEDED(var->getDefaultValueFloat(&default_value_float))) {
							return default_value_float;
						}
						break;
					}
					default: break;
				}
			}
			UtilityFunctions::push_warning(String("Ignoring default value for '%s', only scalar values are currently supported.") % String(var->getName()));
		}
	}
	return nullptr;
}

Variant SlangReflectionContext::to_json() const {
	ERR_FAIL_NULL_V(program_layout, nullptr);
	Slang::ComPtr<slang::IBlob> json_blob;
	if (SLANG_SUCCEEDED(program_layout->toJson(json_blob.writeRef()))) {
		return JSON::parse_string(SlangBlob::blob_to_string(json_blob));
	}
	return nullptr;
}

std::optional<RenderingDevice::UniformType> SlangReflectionContext::_to_godot_uniform_type(const slang::BindingType type, const SlangResourceShape resource_shape) {
	const int64_t base_type = static_cast<int64_t>(type) & SLANG_BINDING_TYPE_BASE_MASK;
	const int64_t type_ext = static_cast<int64_t>(type) & SLANG_BINDING_TYPE_EXT_MASK;
	switch (base_type) {
		case SLANG_BINDING_TYPE_SAMPLER:
			return RenderingDevice::UNIFORM_TYPE_SAMPLER;
		case SLANG_BINDING_TYPE_TEXTURE:
			if (type_ext & SLANG_BINDING_TYPE_MUTABLE_FLAG) {
				return RenderingDevice::UNIFORM_TYPE_IMAGE;
			}
			return RenderingDevice::UNIFORM_TYPE_TEXTURE;
		case SLANG_BINDING_TYPE_COMBINED_TEXTURE_SAMPLER:
			return RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
		case SLANG_BINDING_TYPE_CONSTANT_BUFFER:
			return RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER;
		case SLANG_BINDING_TYPE_TYPED_BUFFER:
		case SLANG_BINDING_TYPE_RAW_BUFFER:
			// TODO: This is weird, do we really need both BindingType and ResourceShape?
			if (resource_shape == SLANG_TEXTURE_BUFFER) {
				return RenderingDevice::UNIFORM_TYPE_TEXTURE_BUFFER;
			}
			return RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		case SLANG_BINDING_TYPE_PARAMETER_BLOCK:
		case SLANG_BINDING_TYPE_PUSH_CONSTANT:
		case SLANG_BINDING_TYPE_INPUT_RENDER_TARGET:
		case SLANG_BINDING_TYPE_INLINE_UNIFORM_DATA:
		case SLANG_BINDING_TYPE_RAY_TRACING_ACCELERATION_STRUCTURE:
			return {};
		default:
			UtilityFunctions::push_warning("Slang: Unknown binding type: ", base_type);
			return {};
	}
}
