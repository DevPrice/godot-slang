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

void SlangShaderImporter::_bind_methods() {}

String SlangShaderImporter::_get_importer_name() const {
	return "slang.shader.importer";
}

String SlangShaderImporter::_get_visible_name() const {
	return "Slang Compute Shader";
}

int32_t SlangShaderImporter::_get_preset_count() const {
	return 1;
}

String SlangShaderImporter::_get_preset_name(int32_t p_preset_index) const {
	return "Compute Shader";
}

PackedStringArray SlangShaderImporter::_get_recognized_extensions() const {
	return PackedStringArray({ "slang" });
}

TypedArray<Dictionary> SlangShaderImporter::_get_import_options(const String& p_path, int32_t p_preset_index) const {
	TypedArray<Dictionary> options{};
	Dictionary default_options{};
	default_options.set("name", "shader_type");
	default_options.set("default_value", 0);
	options.push_back(default_options);
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
	_create_session(session.writeRef());

	const Ref<FileAccess> shader_file = FileAccess::open(p_source_file, FileAccess::READ);
	if (shader_file.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	try {
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
				UtilityFunctions::push_error("Failed to compile Slang shader!");
				return compile_error;
			}
			slang_shader->set_kernels(kernels);
		} else {
			UtilityFunctions::push_error(String("[%s] ") % p_source_file, String("Failed to compile Slang shader:\n%s") % slang_error);
			slang_shader->set_base_error(slang_error);
		}

		const String out_filename = p_save_path + String(".") + _get_save_extension();
		return ResourceSaver::get_singleton()->save(slang_shader, out_filename);
	} catch (...) {
		UtilityFunctions::push_error(String("[%s] Caught exception compiling shader!") % p_source_file);
		return FAILED;
	}
}

SlangResult SlangShaderImporter::_create_session(slang::ISession** out_session) {
	slang::IGlobalSession* global_session = _get_global_session();

	slang::SessionDesc session_desc = {};
	slang::TargetDesc target_desc = {};
	target_desc.format = SLANG_SPIRV;
	target_desc.profile = global_session->findProfile("spirv_1_5");

	session_desc.targets = &target_desc;
	session_desc.targetCount = 1;

	const String extension_path = ProjectSettings::get_singleton()->globalize_path("uid://blqvpxodges3r");
	const String modules_path = extension_path.get_base_dir().path_join("modules");
	char const* search_paths = { modules_path.utf8().get_data() };
	session_desc.searchPaths = &search_paths;
	session_desc.searchPathCount = 1;

	return global_session->createSession(session_desc, out_session);
}

Error SlangShaderImporter::_slang_compile_kernels(slang::IModule* slang_module, TypedArray<ComputeShaderKernel>& out_kernels) {
	for (SlangInt32 entry_point_index = 0; entry_point_index < slang_module->getDefinedEntryPointCount(); ++entry_point_index) {
		Slang::ComPtr<slang::IEntryPoint> entry_point;
		if (slang_module->getDefinedEntryPoint(entry_point_index, entry_point.writeRef()) != OK) {
			UtilityFunctions::push_error(String("[%s] Slang: Error getting entry point '%s'") % Array({ slang_module->getFilePath(), String::num_int64(entry_point_index) }));
			return ERR_BUG;
		}
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
	if (linked_program) {
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

	slang::EntryPointReflection* entry_point_layout = linked_program->getLayout()->getEntryPointByIndex(0);
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

	{
		Dictionary entry_point_attributes{};
		for (size_t attribute_index = 0; attribute_index < entry_point_function->getUserAttributeCount(); ++attribute_index) {
			if (slang::Attribute* attribute = entry_point_function->getUserAttributeByIndex(attribute_index)) {
				Dictionary arguments{};
				for (size_t argument_index = 0; argument_index < attribute->getArgumentCount(); ++argument_index) {
					String argument_name = _get_attribute_argument_name(attribute, argument_index, linked_program->getLayout());
					arguments.set(argument_name, _to_godot_value(attribute, argument_index));
				}
				entry_point_attributes.set(attribute->getName(), arguments);
			}
		}
		kernel->set_user_attributes(entry_point_attributes);
	}

	kernel->set_parameters(_get_param_reflection(linked_program->getLayout(), metadata));
	kernel->set_buffers(_get_buffers_reflection(linked_program->getLayout()));
	return kernel;
}

Dictionary SlangShaderImporter::_get_param_reflection(slang::ProgramLayout* program_layout, slang::IMetadata* metadata) {
	Dictionary parameters{};
	for (size_t param_index = 0; param_index < program_layout->getParameterCount(); ++param_index) {
		slang::VariableLayoutReflection* param = program_layout->getParameterByIndex(param_index);
		Dictionary param_info{};
		param_info.set("name", param->getName());
		Dictionary param_attributes{};
		if (slang::VariableReflection* variable = param->getVariable()) {
			for (size_t attribute_index = 0; attribute_index < variable->getUserAttributeCount(); ++attribute_index) {
				if (slang::Attribute* attribute = variable->getUserAttributeByIndex(attribute_index)) {
					Dictionary arguments{};
					for (size_t argument_index = 0; argument_index < attribute->getArgumentCount(); ++argument_index) {
						String argument_name = _get_attribute_argument_name(attribute, argument_index, program_layout);
						arguments.set(argument_name, _to_godot_value(attribute, argument_index));
					}
					param_attributes.set(attribute->getName(), arguments);
				}
			}
			param_info.set("user_attributes", param_attributes);
		}
		param_info.set("variant_type", _to_godot_type(param->getType(), param_attributes));
		switch (param->getCategory()) {
			case slang::ParameterCategory::DescriptorTableSlot:
				param_info.set("binding_index", param->getBindingIndex());
				param_info.set("binding_space", param->getBindingSpace());
				param_info.set("type", _to_godot_uniform_type(param->getTypeLayout()->getBindingRangeType(0)));
				break;
			case slang::ParameterCategory::Uniform:
				param_info.set("binding_index", program_layout->getGlobalConstantBufferBinding());
				param_info.set("binding_space", param->getBindingSpace());
				param_info.set("type", RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);
				param_info.set("offset", param->getOffset());
				param_info.set("size", param->getTypeLayout()->getSize());
				break;
			default:
				param_info.set("unhandled_category", param->getCategory());
				break;
		}
		parameters.set(param->getName(), param_info);
	}
	return parameters;
}

TypedArray<Dictionary> SlangShaderImporter::_get_buffers_reflection(slang::ProgramLayout* program_layout) {
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

String SlangShaderImporter::_get_attribute_argument_name(slang::Attribute* attribute, const unsigned int argument_index, slang::ProgramLayout* layout) {
	// TODO: Surely there is a better way to do this
	if (attribute && layout) {
		if (slang::TypeReflection* attribute_type = layout->findTypeByName((String(attribute->getName()) + "Attribute").utf8().get_data())) {
			return attribute_type->getFieldByIndex(argument_index)->getName();
		}
	}
	return String("argument") + String::num_int64(argument_index);
}

Variant::Type SlangShaderImporter::_to_godot_type(slang::TypeReflection* type, Dictionary attributes) {
	switch (type->getKind()) {
		case SLANG_TYPE_KIND_SCALAR:
			switch (type->getScalarType()) {
				case SLANG_SCALAR_TYPE_BOOL:
					return Variant::BOOL;
				case SLANG_SCALAR_TYPE_FLOAT16:
				case SLANG_SCALAR_TYPE_FLOAT32:
				case SLANG_SCALAR_TYPE_FLOAT64:
					return Variant::FLOAT;
				case SLANG_SCALAR_TYPE_INT32:
				case SLANG_SCALAR_TYPE_UINT32:
				case SLANG_SCALAR_TYPE_INT64:
				case SLANG_SCALAR_TYPE_UINT64:
				case SLANG_SCALAR_TYPE_INT8:
				case SLANG_SCALAR_TYPE_UINT8:
				case SLANG_SCALAR_TYPE_INT16:
				case SLANG_SCALAR_TYPE_UINT16:
					return Variant::INT;
				default:
					break;
			}
		case SLANG_TYPE_KIND_VECTOR: {
			const bool is_color = attributes.has("gd_Color");
			if (is_color) {
				return Variant::COLOR;
			}
			switch (type->getColumnCount()) {
				case 2: return Variant::VECTOR2;
				case 3: return Variant::VECTOR3;
				case 4: return Variant::VECTOR4;
				default: break;
			}
			break;
		}
		case SLANG_TYPE_KIND_RESOURCE: {
			// TODO: This is not useful as-is
			return Variant::OBJECT;
		}
		case SLANG_TYPE_KIND_STRUCT: {
			if (String(type->getName()) == "String") {
				// TODO: Is there a better way to detect the String type?
				return Variant::STRING;
			}
			break;
		}
		default:
			break;
	}
	// TODO: Support the other types
	UtilityFunctions::push_warning("Slang: Unknown Godot type: ", type->getName(), String(" (%s)") % static_cast<int64_t>(type->getKind()));
	return Variant::NIL;
}

Variant SlangShaderImporter::_to_godot_value(slang::Attribute* attribute, const uint32_t argument_index) {
	if (slang::TypeReflection* type = attribute->getArgumentType(argument_index)) {
		switch (_to_godot_type(type, Dictionary())) {
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
		UtilityFunctions::push_warning("Slang: Failed to make Godot value for attribute: ", type->getName(), String(" (%s)") % static_cast<int64_t>(type->getKind()));
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

RenderingDevice::UniformType SlangShaderImporter::_to_godot_uniform_type(slang::BindingType type) {
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

slang::IGlobalSession* SlangShaderImporter::_get_global_session() {
	static slang::IGlobalSession* global_session = [] {
		slang::IGlobalSession* ptr = nullptr;
		slang::createGlobalSession(&ptr);
		return ptr;
	}();
	return global_session;
}
