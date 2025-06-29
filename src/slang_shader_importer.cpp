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
	if (const Error compile_error = slang_compile_kernels(ProjectSettings::get_singleton()->globalize_path(p_source_file), kernels)) {
		UtilityFunctions::push_error("Failed to compile Slang shader kernels!");
		return compile_error;
	}

	const Ref slang_shader = memnew(ComputeShaderFile);
	slang_shader->set_kernels(kernels);

	const String out_filename = p_save_path + String(".") + _get_save_extension();
	return ResourceSaver::get_singleton()->save(slang_shader, out_filename);
}

Error SlangShaderImporter::slang_compile_kernels(const String &p_source_file, TypedArray<ComputeShaderKernel> &out_kernels) {
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

		slang::EntryPointReflection *entry_point_reflection = linked_program->getLayout()->getEntryPointByIndex(entry_point_index);
		{
			slang::Attribute *shader_attribute = entry_point_reflection->getFunction()->findAttributeByName(global_session, "shader");
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
				UtilityFunctions::push_warning(String("Slang: Skipping compilation of non-compute kernel '%s' (non-compute shader)") % shader_name);
				continue;
			}
		}

		Slang::ComPtr<slang::IBlob> compiled_blob;
		{
			Slang::ComPtr<slang::IBlob> diagnostics_blob;
			const SlangResult result = linked_program->getEntryPointCode(
				0, 0, compiled_blob.writeRef(), diagnostics_blob.writeRef());
			if (result != OK) {
				return FAILED;
			}
		}

		PackedByteArray spirv_bytes{};
		spirv_bytes.resize(compiled_blob->getBufferSize());
		memcpy(spirv_bytes.ptrw(), compiled_blob->getBufferPointer(), compiled_blob->getBufferSize());

		const Ref spirv = memnew(RDShaderSPIRV);
		spirv->set_stage_bytecode(RenderingDevice::SHADER_STAGE_COMPUTE, spirv_bytes);

		const Ref kernel = memnew(ComputeShaderKernel);
		kernel->set_kernel_name(entry_point_reflection->getName());
		kernel->set_spirv(spirv);
		out_kernels.push_back(kernel);
	}

	return OK;
}
