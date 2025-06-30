#include "slang_shader_importer.h"

#include "slang-com-helper.h"
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

void SlangShaderImporter::_bind_methods() { }

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

TypedArray<Dictionary> SlangShaderImporter::_get_import_options(const String &p_path, int32_t p_preset_index) const {
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

bool SlangShaderImporter::_get_option_visibility(const String &p_path, const StringName &p_option_name, const Dictionary &p_options) const {
	return true;
}

Error SlangShaderImporter::_import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options, const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const {
	TypedArray<ComputeShaderKernel> kernels;
	if (const Error compile_error = _slang_compile_kernels(ProjectSettings::get_singleton()->globalize_path(p_source_file), kernels)) {
		UtilityFunctions::push_error("Failed to compile Slang shader kernels!");
		return compile_error;
	}

	const Ref slang_shader = memnew(ComputeShaderFile);
	slang_shader->set_kernels(kernels);

	const String out_filename = p_save_path + String(".") + _get_save_extension();
	return ResourceSaver::get_singleton()->save(slang_shader, out_filename);
}

Error SlangShaderImporter::_slang_compile_kernels(const String &p_source_file, TypedArray<ComputeShaderKernel> &out_kernels) {
	Slang::ComPtr<slang::IGlobalSession> global_session;
	createGlobalSession(global_session.writeRef());

	slang::SessionDesc sessionDesc = {};
	slang::TargetDesc targetDesc = {};
	targetDesc.format = SLANG_SPIRV;
	targetDesc.profile = global_session->findProfile("spirv_1_5");

	sessionDesc.targets = &targetDesc;
	sessionDesc.targetCount = 1;

	Slang::ComPtr<slang::ISession> session;
	global_session->createSession(sessionDesc, session.writeRef());

	const Ref<FileAccess> shader_file = FileAccess::open(p_source_file, FileAccess::READ);
	if (shader_file.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	const String shader_source = shader_file->get_as_text(true);

	Slang::ComPtr<slang::IModule> slang_module;
	{
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		slang_module = session->loadModuleFromSourceString(
				"main_module",
				p_source_file.utf8().get_data(),
				shader_source.utf8().get_data(),
				diagnostics_blob.writeRef());
		if (!slang_module) {
			return FAILED;
		}
	}

	for (size_t entry_point_index = 0; entry_point_index < slang_module->getDefinedEntryPointCount(); ++entry_point_index) {
		Slang::ComPtr<slang::IEntryPoint> entry_point;
		{
			Slang::ComPtr<slang::IBlob> diagnostics_blob;
			slang_module->getDefinedEntryPoint(entry_point_index, entry_point.writeRef());
			if (!entry_point) {
				UtilityFunctions::push_error(String("Slang: Error getting entry point '%s'") % String::num_int64(entry_point_index));
				return FAILED;
			}
		}

		const std::array<slang::IComponentType *, 2> componentTypes = {
			slang_module,
			entry_point
		};

		Slang::ComPtr<slang::IComponentType> composedProgram;
		{
			Slang::ComPtr<slang::IBlob> diagnostics_blob;
			const SlangResult result = session->createCompositeComponentType(
					componentTypes.data(),
					componentTypes.size(),
					composedProgram.writeRef(),
					diagnostics_blob.writeRef());
			if (result != OK) {
				return FAILED;
			}
		}

		Slang::ComPtr<slang::IComponentType> linked_program;
		{
			Slang::ComPtr<slang::IBlob> diagnostics_blob;
			const SlangResult result = composedProgram->link(
					linked_program.writeRef(),
					diagnostics_blob.writeRef());
			if (result != OK) {
				return FAILED;
			}
		}

		const String entry_point_name = entry_point->getFunctionReflection()->getName();

		Slang::ComPtr<slang::IBlob> compiled_blob;
		{
			Slang::ComPtr<slang::IBlob> diagnostics_blob;
			const SlangResult result = linked_program->getEntryPointCode(
				0, 0, compiled_blob.writeRef(), diagnostics_blob.writeRef());
			if (result != OK) {
				return FAILED;
			}
		}

		{
			slang::Attribute *shader_attribute = entry_point->getFunctionReflection()->findAttributeByName(global_session, "shader");
			if (!shader_attribute) {
				UtilityFunctions::push_warning(String("Slang: Skipping compilation of kernel '%s' (no shader attribute)") % entry_point_name);
				continue;
			}
			size_t value_size{};
			const char *shader_value = shader_attribute->getArgumentValueString(0, &value_size);
			if (!shader_value) {
				UtilityFunctions::push_warning(String("Slang: Skipping compilation of kernel '%s' (invalid shader attribute)") % entry_point_name);
				continue;
			}
			const String shader_name = String::utf8(shader_value, value_size);
			if (shader_name != String("compute")) {
				UtilityFunctions::push_warning(String("Slang: Skipping compilation of kernel '%s' (non-compute shader)") % shader_name);
				continue;
			}
		}

		PackedByteArray spirv_bytes{};
		spirv_bytes.resize(compiled_blob->getBufferSize());
		memcpy(spirv_bytes.ptrw(), compiled_blob->getBufferPointer(), compiled_blob->getBufferSize());

		const Ref spirv = memnew(RDShaderSPIRV);
		spirv->set_stage_bytecode(RenderingDevice::SHADER_STAGE_COMPUTE, spirv_bytes);

		const Ref kernel = memnew(ComputeShaderKernel);
		kernel->set_kernel_name(entry_point_name);
		kernel->set_spirv(spirv);

		slang::EntryPointReflection* entry_point_layout = linked_program->getLayout()->getEntryPointByIndex(entry_point_index);
		{
			SlangUInt sizes[3];
			entry_point_layout->getComputeThreadGroupSize(3, sizes);
			kernel->set_thread_group_size(Vector3(sizes[0], sizes[1], sizes[2]));
		}

		Dictionary entry_point_attributes{};
		for (size_t attribute_index = 0; attribute_index < entry_point->getFunctionReflection()->getUserAttributeCount(); ++attribute_index) {
			if (slang::Attribute* attribute = entry_point->getFunctionReflection()->getUserAttributeByIndex(attribute_index)) {
				Dictionary arguments{};
				for (size_t argument_index = 0; argument_index < attribute->getArgumentCount(); ++argument_index) {
					String argument_name = _get_attribute_argument_name(attribute, argument_index, linked_program->getLayout());
					arguments.set(argument_name, _to_godot_value(attribute, argument_index));
				}
				entry_point_attributes.set(attribute->getName(), arguments);
			}
		}
		kernel->set_user_attributes(entry_point_attributes);

		out_kernels.push_back(kernel);
	}

	return OK;
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

Variant::Type SlangShaderImporter::_to_godot_type(slang::TypeReflection* type) {
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
				default: return Variant::NIL;
			}
		case SLANG_TYPE_KIND_STRUCT:
			if (String(type->getName()) == "String") {
				// TODO: Is there a better way to detect the String type?
				return Variant::STRING;
			}
			break;
		// TODO: Support the other types
		default: return Variant::NIL;
	}
	return Variant::NIL;
}

Variant SlangShaderImporter::_to_godot_value(slang::Attribute* attribute, const uint32_t argument_index) {
	if (slang::TypeReflection* type = attribute->getArgumentType(argument_index)) {
		switch (_to_godot_type(type)) {
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
			default: return Variant{};
		}
	}
	return Variant{};
}
