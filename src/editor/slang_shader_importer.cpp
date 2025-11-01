#include "slang_shader_importer.h"

#include "slang-com-ptr.h"
#include "slang.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_saver.hpp>

#include <compute_shader_file.h>
#include <compute_shader_kernel.h>

#include "godot_cpp/classes/texture2d.hpp"

void SlangShaderImporter::_bind_methods() {
	BIND_ENUM_CONSTANT(ROW_MAJOR)
	BIND_ENUM_CONSTANT(COLUMN_MAJOR)
}

String SlangShaderImporter::_get_importer_name() const {
	return "slang.shader.importer";
}

String SlangShaderImporter::_get_visible_name() const {
	return "Compute Shader";
}

int32_t SlangShaderImporter::_get_preset_count() const {
	return 1;
}

String SlangShaderImporter::_get_preset_name(int32_t p_preset_index) const {
	return "Compute Shader";
}

PackedStringArray SlangShaderImporter::_get_recognized_extensions() const {
	return PackedStringArray({ "slang", "hlsl", "glsl" });
}

TypedArray<Dictionary> SlangShaderImporter::_get_import_options(const String& p_path, int32_t p_preset_index) const {
	TypedArray<Dictionary> options{};
	{
		Dictionary entry_points_option{};
		entry_points_option.set("name", "entry_points");
		entry_points_option.set("default_value", PackedStringArray());
		options.push_back(entry_points_option);
	}
	{
		Dictionary matrix_layout_option{};
		matrix_layout_option.set("name", "matrix_layout");
		matrix_layout_option.set("default_value", ROW_MAJOR);
		matrix_layout_option.set("property_hint", PROPERTY_HINT_ENUM);
		matrix_layout_option.set("hint_string", String("RowMajor:%s,ColumnMajor:%s") % Array { String::num_int64(ROW_MAJOR), String::num_int64(COLUMN_MAJOR) });
		options.push_back(matrix_layout_option);
	}
	return options;
}

String SlangShaderImporter::_get_save_extension() const {
	return "res";
}

String SlangShaderImporter::_get_resource_type() const {
	return "ComputeShaderFile";
}

float SlangShaderImporter::_get_priority() const {
	return 1.f;
}

int32_t SlangShaderImporter::_get_import_order() const {
	return 0;
}

bool SlangShaderImporter::_get_option_visibility(const String& p_path, const StringName& p_option_name, const Dictionary& p_options) const {
	return true;
}

Error SlangShaderImporter::_import(const String& p_source_file, const String& p_save_path, const Dictionary& p_options, const TypedArray<String>& p_platform_variants, const TypedArray<String>& p_gen_files) const {
	Slang::ComPtr<slang::ISession> session;
	if (SLANG_FAILED(_create_session(session.writeRef(), p_options, p_source_file.ends_with(".glsl")))) {
		return FAILED;
	}

	const Ref<FileAccess> shader_file = FileAccess::open(p_source_file, FileAccess::READ);
	if (shader_file.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	const String shader_source = shader_file->get_as_text(true);
	String slang_error{};

	Slang::ComPtr<slang::IModule> slang_module;
	{
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		slang_module = session->loadModuleFromSourceString(
				"__main_module",
				p_source_file.get_file().utf8().get_data(),
				shader_source.utf8().get_data(),
				diagnostics_blob.writeRef());
		if (!slang_module) {
			slang_error = String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize());
		}
	}

	const Ref slang_shader = memnew(ComputeShaderFile);
	if (slang_module && slang_error.is_empty()) {
		TypedArray<ComputeShaderKernel> kernels;
		if (const Error compile_error = _slang_compile_kernels(slang_module, kernels)) {
			ERR_FAIL_V_MSG(compile_error, "Failed to compile Slang shader!");
		}
		slang_shader->set_kernels(kernels);
	} else {
		UtilityFunctions::push_error(String("[%s] ") % p_source_file, String("Failed to compile Slang shader:\n%s") % slang_error);
		slang_shader->set_base_error(slang_error);
	}

	const String out_filename = p_save_path + String(".") + _get_save_extension();
	return ResourceSaver::get_singleton()->save(slang_shader, out_filename);
}

SlangResult SlangShaderImporter::_create_session(slang::ISession** out_session, const Dictionary& options, const bool enable_glsl) {
	slang::IGlobalSession* global_session = _get_global_session(enable_glsl);
	ERR_FAIL_NULL_V_MSG(global_session, SLANG_FAIL, "Failed to initialize global Slang session!");

	slang::SessionDesc session_desc = {};
	slang::TargetDesc target_desc = {};
	target_desc.format = SLANG_SPIRV;
	target_desc.profile = global_session->findProfile("spirv_1_5");

	session_desc.targets = &target_desc;
	session_desc.targetCount = 1;

	std::vector<slang::CompilerOptionEntry> compiler_options{};

	PackedStringArray entry_points = options["entry_points"];
	for (const String& entry_point : entry_points) {
		// TODO: This doesn't actually work
		constexpr auto compute_stage = "compute";
		slang::CompilerOptionValue entry_point_value{};
		entry_point_value.kind = slang::CompilerOptionValueKind::String;
		entry_point_value.stringValue0 = entry_point.utf8().get_data();
		const slang::CompilerOptionEntry entry_point_option = {slang::CompilerOptionName::EntryPointName, entry_point_value};
		compiler_options.push_back(entry_point_option);
		slang::CompilerOptionValue stage_value{};
		stage_value.kind = slang::CompilerOptionValueKind::String;
		stage_value.stringValue0 = compute_stage;
		const slang::CompilerOptionEntry stage_option = {slang::CompilerOptionName::Stage, stage_value};
		compiler_options.push_back(stage_option);
	}

	session_desc.compilerOptionEntries = compiler_options.data();
	session_desc.compilerOptionEntryCount = compiler_options.size();

	const int64_t matrix_layout = options["matrix_layout"];
	if (matrix_layout >= SLANG_MATRIX_LAYOUT_ROW_MAJOR && matrix_layout <= SLANG_MATRIX_LAYOUT_COLUMN_MAJOR) {
		session_desc.defaultMatrixLayoutMode = static_cast<SlangMatrixLayoutMode>(matrix_layout);
	}

	const String extension_path = ProjectSettings::get_singleton()->globalize_path("uid://blqvpxodges3r");
	const String modules_path = extension_path.get_base_dir().path_join("modules");
	char const* search_paths = { modules_path.utf8().get_data() };
	session_desc.searchPaths = &search_paths;
	session_desc.searchPathCount = 1;

	return global_session->createSession(session_desc, out_session);
}

Error SlangShaderImporter::_slang_compile_kernels(slang::IModule* slang_module, TypedArray<ComputeShaderKernel>& out_kernels) {
	for (int32_t entry_point_index = 0; entry_point_index < slang_module->getDefinedEntryPointCount(); ++entry_point_index) {
		Slang::ComPtr<slang::IEntryPoint> entry_point;
		ERR_FAIL_COND_V_MSG(
			slang_module->getDefinedEntryPoint(entry_point_index, entry_point.writeRef()) != OK,
			ERR_BUG,
			String("[%s] Slang: Error getting entry point '%s'") % Array({ slang_module->getFilePath(), String::num_int64(entry_point_index) }));
		const Ref<ComputeShaderKernel> kernel = _slang_compile_kernel(slang_module->getSession(), slang_module, entry_point);
		if (kernel.is_valid()) {
			const String compile_error = kernel->get_compile_error();
			if (!compile_error.is_empty()) {
				UtilityFunctions::push_error(String("[%s] Slang compile error:\n%s") % Array({ slang_module->getFilePath(), compile_error }));
			}
			out_kernels.push_back(kernel);
		}
	}

	return OK;
}

Ref<ComputeShaderKernel> SlangShaderImporter::_slang_compile_kernel(slang::ISession* session, slang::IModule* slang_module, slang::IEntryPoint* entry_point) {
	String compile_error{};

	const Ref kernel = memnew(ComputeShaderKernel);

	const auto entry_point_function = entry_point->getFunctionReflection();
	const String entry_point_name = entry_point_function->getName();
	kernel->set_kernel_name(entry_point_name);

	const Ref spirv = memnew(RDShaderSPIRV);
	kernel->set_spirv(spirv);

	Slang::ComPtr<slang::IComponentType> composed_program;
	{
		const std::array<slang::IComponentType*, 2> componentTypes = {
			slang_module,
			entry_point,
		};
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		const SlangResult result = session->createCompositeComponentType(
				componentTypes.data(),
				componentTypes.size(),
				composed_program.writeRef(),
				diagnostics_blob.writeRef());
		if (result != OK) {
			compile_error = String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize());
		} else if (diagnostics_blob) {
			UtilityFunctions::push_warning("Slang (program): ", String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()));
		}
	}

	Slang::ComPtr<slang::IComponentType> linked_program;
	if (composed_program) {
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		const SlangResult result = composed_program->link(
				linked_program.writeRef(),
				diagnostics_blob.writeRef());
		if (result != OK) {
			compile_error = String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize());
			spirv->set_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE, compile_error);
			return kernel;
		}
	}

	Slang::ComPtr<slang::IBlob> compiled_blob;
	{
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		const SlangResult result = linked_program->getEntryPointCode(
				0, 0, compiled_blob.writeRef(), diagnostics_blob.writeRef());
		if (result != OK) {
			compile_error = String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize());
		} else if (diagnostics_blob) {
			// TODO: ignore these until we figure out:
			// (0): error 100: failed to load downstream compiler 'spirv-opt'
			// (0): note 99999: failed to load dynamic library 'slang-glslang'
			// UtilityFunctions::push_warning("Slang (link): ", String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()));
		}
	}

	slang::ProgramLayout* program_layout = linked_program->getLayout();

	slang::EntryPointReflection* entry_point_layout = program_layout->getEntryPointByIndex(0);
	{
		if (entry_point_layout->getStage() != SLANG_STAGE_COMPUTE) {
			UtilityFunctions::push_warning(String("Slang: Skipping compilation of kernel '%s' (non-compute shader)") % entry_point_name);
			return nullptr;
		}
	}

	slang::IMetadata* metadata;
	{
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		const SlangResult result = linked_program->getEntryPointMetadata(
				0, 0, &metadata, diagnostics_blob.writeRef());
		if (result != OK) {
			compile_error = String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize());
		} else if (diagnostics_blob) {
			UtilityFunctions::push_warning("Slang (metadata): ", String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()));
		}
	}

	if (compile_error.is_empty()) {
		PackedByteArray spirv_bytes{};
		spirv_bytes.resize(compiled_blob->getBufferSize());
		memcpy(spirv_bytes.ptrw(), compiled_blob->getBufferPointer(), compiled_blob->getBufferSize());
		spirv->set_stage_bytecode(RenderingDevice::SHADER_STAGE_COMPUTE, spirv_bytes);
	} else {
		spirv->set_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE, compile_error);
		return kernel;
	}

	{
		SlangUInt sizes[3];
		entry_point_layout->getComputeThreadGroupSize(3, sizes);
		kernel->set_thread_group_size(Vector3i(sizes[0], sizes[1], sizes[2]));
	}

	SlangReflectionContext reflection_context(program_layout);
	kernel->set_user_attributes(reflection_context.get_attributes(entry_point_function));
	kernel->set_parameters(reflection_context.get_param_reflection(metadata));
	kernel->set_buffers(reflection_context.get_buffers_reflection());
	return kernel;
}

Dictionary SlangReflectionContext::get_param_reflection(slang::IMetadata* metadata) const {
	Dictionary parameters{};
	size_t push_constant_offset = 0;
	for (size_t param_index = 0; param_index < program_layout->getParameterCount(); ++param_index) {
		slang::VariableLayoutReflection* param = program_layout->getParameterByIndex(param_index);
		Dictionary param_info{};
		Dictionary param_attributes = get_attributes(param->getVariable());
		const StringName param_name = get_name(param, param_attributes);
		param_info.set("name", param_name);

		param_info.set("user_attributes", param_attributes);

		const bool is_autobind = _is_autobind(param->getVariable());
		const Dictionary shape = _get_shape(param->getTypeLayout(), !is_autobind);
		const bool is_valid_shape = !shape.is_empty() &&
			(shape["type"] != "simple" || static_cast<int64_t>(shape.get("size", -1)) != 0);
		if (!is_valid_shape) {
			continue;
		}

		param_info.set("shape", shape);

		Variant::Type type;
		PropertyHint hint;
		String hint_string;
		if (_get_godot_type(param->getType(), param_attributes, type, hint, hint_string)) {
			const uint32_t usage = is_autobind ? PROPERTY_USAGE_NONE : PROPERTY_USAGE_DEFAULT;
			PropertyInfo property_info{type, param_name, hint, hint_string, usage};
			param_info.set("property_info", Dictionary(property_info));
		}

		switch (param->getCategory()) {
			case slang::ParameterCategory::DescriptorTableSlot:
				param_info.set("binding_index", param->getBindingIndex());
				param_info.set("binding_space", param->getBindingSpace());
				param_info.set("uniform_type", _to_godot_uniform_type(param->getTypeLayout()->getBindingRangeType(0)));
				break;
			case slang::ParameterCategory::Uniform:
				param_info.set("binding_index", program_layout->getGlobalConstantBufferBinding());
				param_info.set("binding_space", param->getBindingSpace());
				param_info.set("uniform_type", RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);
				param_info.set("offset", param->getOffset());
				break;
			case slang::ParameterCategory::PushConstantBuffer: {
				slang::TypeLayoutReflection* element_type_layout = param->getTypeLayout()->getElementTypeLayout();
				param_info.set("offset", push_constant_offset);
				// TODO: Smells like a hack, but I can't figure out where to fetch this otherwise
				// Getting mixed signals from Slang on whether more than one parameter can be bound to the push constant buffer
				// Maybe can remove this, under the assumption that the offset is always zero?
				push_constant_offset += element_type_layout->getSize();
				break;
			}
			default:
				param_info.set("unhandled_category", param->getCategory());
				break;
		}

		parameters.set(param_name, param_info);
	}
	return parameters;
}

Dictionary SlangReflectionContext::_get_shape(slang::TypeLayoutReflection* type_layout, const bool include_property_info) const {
	Dictionary shape{};
	if (!type_layout) return shape;

	switch (type_layout->getKind()) {
		case slang::TypeReflection::Kind::Scalar:
		case slang::TypeReflection::Kind::Vector:
		case slang::TypeReflection::Kind::Matrix:
			shape.set("type", "simple");
			shape.set("size", type_layout->getSize());
			break;
		case slang::TypeReflection::Kind::Struct: {
			shape.set("type", "structured");
			shape.set("size", type_layout->getSize());
			const int32_t alignment = type_layout->getAlignment();
			if (alignment > 0) {
				shape.set("alignment", alignment);
			}
			Dictionary property_shapes{};
			for (int i = 0; i < type_layout->getFieldCount(); i++) {
				slang::VariableLayoutReflection* field = type_layout->getFieldByIndex(i);
				Dictionary property{};
				const bool is_autobind = _is_autobind(field->getVariable());
				Dictionary property_shape = _get_shape(field->getTypeLayout(), include_property_info && !is_autobind);
				const Dictionary field_attributes = get_attributes(field->getVariable());
				property.set("shape", property_shape);
				property.set("user_attributes", field_attributes);
				property.set("offset", field->getOffset());
				property_shapes.set(get_name(field, field_attributes), property);

				if (include_property_info) {
					Variant::Type type;
					PropertyHint hint;
					String hint_string;
					if (_get_godot_type(field->getType(), field_attributes, type, hint, hint_string)) {
						const uint32_t usage = is_autobind ? PROPERTY_USAGE_NONE : PROPERTY_USAGE_DEFAULT;
						PropertyInfo property_info{type, get_name(field, field_attributes), hint, hint_string, usage};
						property.set("property_info", Dictionary(property_info));
					}
				}
			}
			shape.set("properties", property_shapes);
			break;
		}
		case slang::TypeReflection::Kind::Resource: {
			const SlangResourceShapeIntegral base_resource_shape = type_layout->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK;
			if (base_resource_shape == SLANG_BYTE_ADDRESS_BUFFER) {
				shape.set("type", "raw_bytes");
				break;
			}
			if (base_resource_shape != SLANG_STRUCTURED_BUFFER) {
				shape.set("type", "resource");
				break;
			}
		}
		case slang::TypeReflection::Kind::Array:
		case slang::TypeReflection::Kind::ShaderStorageBuffer: {
			shape.set("type", "array");
			shape.set("element_shape", _get_shape(type_layout->getElementTypeLayout(), include_property_info));
			const size_t stride = type_layout->getElementTypeLayout()->getStride();
			if (stride > 0) {
				shape.set("stride", stride);
			}
			const int32_t alignment = type_layout->getElementTypeLayout()->getAlignment();
			if (alignment > 0) {
				shape.set("alignment", alignment);
			}
			if (type_layout->isArray()) {
				const size_t size = stride * type_layout->getElementCount();
				if (size > 0) {
					shape.set("size", type_layout->getSize());
				}
			}
			break;
		}
		case slang::TypeReflection::Kind::ConstantBuffer:
			return _get_shape(type_layout->getElementTypeLayout(), include_property_info);
		default:
			break;
	}

	shape.set("user_attributes", get_attributes(type_layout->getType()));

	const SlangMatrixLayoutMode matrix_layout = type_layout->getMatrixLayoutMode();
	if (matrix_layout != SLANG_MATRIX_LAYOUT_MODE_UNKNOWN) {
		shape.set("matrix_layout", type_layout->getMatrixLayoutMode());
	}

	return shape;
}

bool SlangReflectionContext::_is_autobind(slang::VariableReflection* var) const {
	for (size_t attribute_index = 0; attribute_index < var->getUserAttributeCount(); ++attribute_index) {
		if (slang::Attribute* attribute = var->getUserAttributeByIndex(attribute_index)) {
			if (slang::TypeReflection* attribute_type = _get_attribute_type(attribute)) {
				if (attribute_type->findUserAttributeByName("gd_Autobind")) {
					return true;
				}
			}
		}
	}
	return false;
}

TypedArray<Dictionary> SlangReflectionContext::get_buffers_reflection() const {
	TypedArray<Dictionary> buffers{};
	const size_t global_buffer_size = program_layout->getGlobalConstantBufferSize();
	if (global_buffer_size > 0) {
		Dictionary buffer_info{};
		buffer_info.set("binding_index", program_layout->getGlobalParamsVarLayout()->getBindingIndex());
		buffer_info.set("binding_space", program_layout->getGlobalParamsVarLayout()->getBindingSpace());
		buffer_info.set("size", global_buffer_size);
		buffers.push_back(buffer_info);
	}
	for (size_t param_index = 0; param_index < program_layout->getParameterCount(); ++param_index) {
		slang::VariableLayoutReflection* param = program_layout->getParameterByIndex(param_index);
		if (slang::TypeLayoutReflection* type_layout = param->getTypeLayout()) {
			if (param->getCategory() == slang::DescriptorTableSlot && type_layout->getKind() == slang::TypeReflection::Kind::ConstantBuffer) {
				Dictionary buffer_info{};
				buffer_info.set("binding_index", param->getBindingIndex());
				buffer_info.set("binding_space", param->getBindingSpace());
				buffer_info.set("size", type_layout->getElementTypeLayout()->getSize());
				buffers.push_back(buffer_info);
			}
		}
	}
	return buffers;
}

// TODO: Surely there is a better way to do this
slang::TypeReflection* SlangReflectionContext::_get_attribute_type(slang::Attribute* attribute) const {
	if (attribute) {
		return program_layout->findTypeByName((String(attribute->getName()) + "Attribute").utf8().get_data());
	}
	return nullptr;
}

String SlangReflectionContext::_get_attribute_argument_name(slang::Attribute* attribute, const unsigned int argument_index) const {
	if (attribute) {
		if (slang::TypeReflection* attribute_type = _get_attribute_type(attribute)) {
			slang::VariableReflection* field = attribute_type->getFieldByIndex(argument_index);
			return get_name(field, get_attributes(field));
		}
	}
	return String("argument") + String::num_int64(argument_index);
}

 bool SlangReflectionContext::_get_godot_type(slang::TypeReflection* type, const Dictionary& attributes, Variant::Type& out_type, PropertyHint& out_hint, String& out_hint_string) const {
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
		case slang::TypeReflection::Kind::Vector: {
			const bool is_color = attributes.has("gd_Color");
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
		case slang::TypeReflection::Kind::Resource: {
			switch (type->getResourceShape() & ~SLANG_TEXTURE_COMBINED_FLAG) {
				case SLANG_TEXTURE_2D:
					out_type = Variant::OBJECT;
					out_hint = PROPERTY_HINT_RESOURCE_TYPE;
					out_hint_string = Texture2D::get_class_static();
					return true;
				case SLANG_BYTE_ADDRESS_BUFFER: {
					out_type = Variant::PACKED_BYTE_ARRAY;
					return true;
				}
				case SLANG_STRUCTURED_BUFFER: {
					out_type = Variant::ARRAY;

					Variant::Type element_type;
					PropertyHint element_hint;
					String element_hint_string;

					if (_get_godot_type(type->getElementType(), attributes, element_type, element_hint, element_hint_string)) {
						switch (element_type) {
							case Variant::INT:
								if (type->getElementType()->getScalarType() == slang::TypeReflection::ScalarType::Int64) {
									out_type = Variant::PACKED_INT64_ARRAY;
								} else {
									out_type = Variant::PACKED_INT32_ARRAY;
								}
								break;
							case Variant::FLOAT:
								if (type->getElementType()->getScalarType() == slang::TypeReflection::ScalarType::Float64) {
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
								out_hint = PROPERTY_HINT_ARRAY_TYPE;
								out_hint_string = Variant::get_type_name(element_type);
								break;
						}
						return true;
					}
					return false;
				}
				default:
					break;
			}
			break;
		}
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
			const Dictionary class_args = type_attributes["gd_Class"];
			const StringName class_name = class_args["class_name"];
			// TODO: Verify the class_name is a Resource subtype
			if (!class_name.is_empty()) {
				out_type = Variant::OBJECT;
				out_hint = PROPERTY_HINT_RESOURCE_TYPE;
				out_hint_string = class_name;
				return true;
			}
			return false;
		}
		case slang::TypeReflection::Kind::SamplerState:
			// Not currently supported
			return false;
		default:
			break;
	}
	// TODO: Support the other types
	UtilityFunctions::push_warning("Slang: Unknown Godot type: ", type->getName(), String(" (%s)") % static_cast<int64_t>(type->getKind()));
	return false;
}

Variant SlangReflectionContext::_to_godot_value(slang::Attribute* attribute, const uint32_t argument_index) const {
	if (slang::TypeReflection* type_reflection = attribute->getArgumentType(argument_index)) {
		PropertyHint hint{};
		Variant::Type type{};
		String hint_string{};
		_get_godot_type(type_reflection, Dictionary(), type, hint, hint_string);
		switch (type) {
			case Variant::BOOL: {
				int value{};
				if (attribute->getArgumentValueInt(argument_index, &value) == SLANG_OK) {
					return static_cast<bool>(value);
				}
				break;
			}
			case Variant::INT: {
				int value{};
				if (attribute->getArgumentValueInt(argument_index, &value) == SLANG_OK) {
					return value;
				}
				break;
			}
			case Variant::FLOAT: {
				float value{};
				if (attribute->getArgumentValueFloat(argument_index, &value) == SLANG_OK) {
					return value;
				}
				break;
			}
			case Variant::STRING: {
				size_t size{};
				if (const char* value = attribute->getArgumentValueString(argument_index, &size)) {
					return String::utf8(value, size);
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
		if (attribute->getArgumentValueInt(argument_index, &value) == SLANG_OK) {
			// TODO: This happens for enum types? Look into this
			return value;
		}
		UtilityFunctions::push_error("Slang: Got null type pointer for attribute ", attribute->getName());
	}
	return Variant{};
}

RenderingDevice::UniformType SlangReflectionContext::_to_godot_uniform_type(slang::BindingType type) {
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
			return RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		case SLANG_BINDING_TYPE_PARAMETER_BLOCK:
		case SLANG_BINDING_TYPE_INPUT_RENDER_TARGET:
		case SLANG_BINDING_TYPE_INLINE_UNIFORM_DATA:
		case SLANG_BINDING_TYPE_RAY_TRACING_ACCELERATION_STRUCTURE:
		default:
			UtilityFunctions::push_warning("Unknown binding type: ", base_type);
			return static_cast<RenderingDevice::UniformType>(-1);
	}
}

slang::IGlobalSession* SlangShaderImporter::_get_global_session(const bool enable_glsl) {
	static bool glsl_initialized = enable_glsl;
	static Slang::ComPtr<slang::IGlobalSession> global_session = [enable_glsl] {
		Slang::ComPtr<slang::IGlobalSession> ptr;
		SlangGlobalSessionDesc desc = {};
		desc.enableGLSL = enable_glsl;
		slang::createGlobalSession(&desc, ptr.writeRef());
		return ptr;
	}();

	if (!glsl_initialized && enable_glsl) {
		SlangGlobalSessionDesc desc = {};
		desc.enableGLSL = true;
		slang::createGlobalSession(&desc, global_session.writeRef());
		glsl_initialized = true;
	}

	return global_session;
}
