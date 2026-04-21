#include "godot_cpp/classes/project_settings.hpp"

#include "compute_shader_cursor.h"
#include "compute_shader_file.h"
#include "reflection_context.h"
#include "slang_entry_point.h"

#include "slang_module.h"

using namespace gdslang;
using namespace godot;

void SlangModule::_bind_methods() {
	ClassDB::bind_method(D_METHOD("compile_shader", "additional_entry_points"), &SlangModule::compile_shader, DEFVAL(PackedStringArray{}));
	BIND_METHOD(SlangModule, get_params_shape)
	BIND_METHOD(SlangModule, to_json)
	BIND_METHOD(SlangModule, get_dependency_files)
	BIND_METHOD(SlangModule, get_defined_entry_point_count)
	BIND_METHOD(SlangModule, get_defined_entry_point, "index")
	BIND_METHOD(SlangModule, find_entry_point, "entry_point_name")
	BIND_METHOD(SlangModule, find_and_check_entry_point, "entry_point_name", "shader_stage")
}

slang::IModule* SlangModule::get_module() const {
	return module.get();
}

slang::IModule** SlangModule::get_write_ref() {
	return module.writeRef();
}

slang::IComponentType* SlangModule::get_component_type() const {
	return get_module();
}

String SlangModule::get_file_path() const {
	ERR_FAIL_NULL_V(module, {});
	return module->getFilePath();
}

PackedStringArray SlangModule::get_dependency_files() const {
	ERR_FAIL_NULL_V(module, {});
	PackedStringArray dependency_files{};
	for (int32_t i = 0; i < module->getDependencyFileCount(); ++i) {
		const String localized_path = ProjectSettings::get_singleton()->localize_path(module->getDependencyFilePath(i));
		dependency_files.push_back(localized_path);
	}
	return dependency_files;
}

TypedArray<Ref<ComputeShaderKernel>> SlangModule::compile_kernels(const PackedStringArray& additional_entry_points) {
	TypedArray<Ref<ComputeShaderKernel>> kernels{};
	const Ref<ShaderTypeLayoutShape> global_params_shape = get_params_shape();
	const Error err = _compile_kernels(kernels, global_params_shape, additional_entry_points);
	if (err != OK) {
		ERR_PRINT(String("Failed to compile kernels: %s") % UtilityFunctions::error_string(err));
	}
	return kernels;
}

Error SlangModule::_compile_kernels(TypedArray<Ref<ComputeShaderKernel>>& out_kernels, const Ref<ShaderTypeLayoutShape>& global_params_shape, const PackedStringArray& additional_entry_points) {
	ERR_FAIL_NULL_V(module, ERR_UNCONFIGURED);
	std::vector<Slang::ComPtr<slang::IEntryPoint>> entry_points{};
	entry_points.reserve(module->getDefinedEntryPointCount() + additional_entry_points.size());
	if (module->getDefinedEntryPointCount() == 0 && additional_entry_points.is_empty()) {
		Slang::ComPtr<slang::IEntryPoint> entry_point;
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		if (SLANG_SUCCEEDED(module->findAndCheckEntryPoint("main", SlangStage::SLANG_STAGE_COMPUTE, entry_point.writeRef(), diagnostics_blob.writeRef()))) {
			entry_points.push_back(entry_point);
		} else if (diagnostics_blob) {
			UtilityFunctions::push_error(SlangBlob::blob_to_string(diagnostics_blob));
		}
	} else {
		for (int32_t entry_point_index = 0; entry_point_index < module->getDefinedEntryPointCount(); ++entry_point_index) {
			Slang::ComPtr<slang::IEntryPoint> entry_point;
			ERR_FAIL_COND_V_MSG(
					module->getDefinedEntryPoint(entry_point_index, entry_point.writeRef()) != OK,
					ERR_BUG,
					String("[%s] Slang: Error getting entry point '%s'") % Array({ module->getFilePath(), String::num_int64(entry_point_index) }));
			entry_points.push_back(entry_point);
		}
		for (const String& entry_point_name : additional_entry_points) {
			const CharString name_string = entry_point_name.utf8();
			Slang::ComPtr<slang::IEntryPoint> entry_point;
			Slang::ComPtr<slang::IBlob> diagnostics_blob;
			if (SLANG_SUCCEEDED(module->findAndCheckEntryPoint(name_string.get_data(), SlangStage::SLANG_STAGE_COMPUTE, entry_point.writeRef(), diagnostics_blob.writeRef()))) {
				entry_points.push_back(entry_point);
			} else {
				Ref<ComputeShaderKernel> kernel;
				kernel.instantiate();
				Ref<RDShaderSPIRV> spirv;
				spirv.instantiate();
				if (diagnostics_blob && diagnostics_blob->getBufferSize() > 0) {
					spirv->set_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE, SlangBlob::blob_to_string(diagnostics_blob).trim_suffix("\n"));
				} else {
					spirv->set_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE, String("Failed to find entry point '%s'!") % entry_point_name);
				}
				kernel->set_kernel_name(entry_point_name);
				kernel->set_spirv(spirv);
				out_kernels.push_back(kernel);
			}
		}
	}
	for (const Slang::ComPtr<slang::IEntryPoint>& entry_point : entry_points) {
		const Ref<ComputeShaderKernel> kernel = _compile_kernel(entry_point, global_params_shape);
		if (kernel.is_valid()) {
			out_kernels.push_back(kernel);
		}
	}

	return OK;
}

Ref<ComputeShaderFile> SlangModule::compile_shader(const PackedStringArray& additional_entry_points) {
	const Ref slang_shader = memnew(ComputeShaderFile);
	const String diagnostic = get_diagnostic();
	if (diagnostic.is_empty()) {
		const Ref<StructTypeLayoutShape> global_params = get_params_shape();
		slang_shader->set_parameters(global_params);
		TypedArray<Ref<ComputeShaderKernel>> kernels;
		if (const Error compile_error = _compile_kernels(kernels, global_params.ptr(), additional_entry_points)) {
			slang_shader->set_base_error(UtilityFunctions::error_string(compile_error));
		} else if (kernels.is_empty()) {
			slang_shader->set_base_error("No entry points found!");
		} else {
			slang_shader->set_kernels(kernels);
		}
	} else {
		slang_shader->set_base_error(diagnostic);
	}

	slang_shader->set_meta("godot_version", ComputeShaderFile::get_godot_version_string());
	return slang_shader;
}

int64_t SlangModule::get_defined_entry_point_count() const {
	ERR_FAIL_NULL_V(module, 0);
	return module->getDefinedEntryPointCount();
}

Ref<SlangEntryPoint> SlangModule::get_defined_entry_point(const int64_t index) const {
	ERR_FAIL_NULL_V(module, nullptr);
	ERR_FAIL_INDEX_V(index, module->getDefinedEntryPointCount(), nullptr);
	Ref entry_point = memnew(SlangEntryPoint);
	Slang::ComPtr<slang::IBlob> diagnostics_blob;
	ERR_FAIL_COND_V(SLANG_FAILED(module->getDefinedEntryPoint(index, entry_point->write_ref())), nullptr);
	return entry_point;
}

Ref<SlangEntryPoint> SlangModule::find_entry_point(const String& name) const {
	ERR_FAIL_NULL_V(module, nullptr);
	Ref entry_point = memnew(SlangEntryPoint);
	Slang::ComPtr<slang::IBlob> diagnostics_blob;
	const CharString entry_point_name = name.utf8();
	ERR_FAIL_COND_V(SLANG_FAILED(module->findEntryPointByName(entry_point_name.get_data(), entry_point->write_ref())), nullptr);
	return entry_point;
}

Ref<SlangEntryPoint> SlangModule::find_and_check_entry_point(const String& name, const RenderingDevice::ShaderStage shader_stage) const {
	ERR_FAIL_NULL_V(module, nullptr);
	ERR_FAIL_COND_V(shader_stage != RenderingDevice::SHADER_STAGE_COMPUTE, nullptr);
	Ref entry_point = memnew(SlangEntryPoint);
	Slang::ComPtr<slang::IBlob> diagnostics_blob;
	const CharString entry_point_name = name.utf8();
	ERR_FAIL_COND_V(SLANG_FAILED(module->findAndCheckEntryPoint(entry_point_name.get_data(), SlangStage::SLANG_STAGE_COMPUTE, entry_point->write_ref(), diagnostics_blob.writeRef())), nullptr);
	if (diagnostics_blob) {
		entry_point->set_diagnostic(SlangBlob::blob_to_string(diagnostics_blob));
	}
	return entry_point;
}

Ref<ComputeShaderKernel> SlangModule::_compile_kernel(slang::IEntryPoint* entry_point, const Ref<ShaderTypeLayoutShape>& global_params_shape) {
	ERR_FAIL_NULL_V(module, nullptr);
	slang::ISession* session = module->getSession();

	String compile_error{};
	Slang::ComPtr<slang::IComponentType> composed_program;
	{
		const std::array<slang::IComponentType*, 2> componentTypes = {
			module.get(),
			entry_point,
		};
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		const SlangResult result = session->createCompositeComponentType(
				componentTypes.data(),
				componentTypes.size(),
				composed_program.writeRef(),
				diagnostics_blob.writeRef());
		if (result != OK) {
			compile_error = SlangBlob::blob_to_string(diagnostics_blob);
		} else if (diagnostics_blob) {
			UtilityFunctions::push_warning("Slang (program): ", SlangBlob::blob_to_string(diagnostics_blob));
		}
	}

	Slang::ComPtr<slang::IComponentType> linked_program;
	if (composed_program) {
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		const SlangResult result = composed_program->link(
				linked_program.writeRef(),
				diagnostics_blob.writeRef());
		if (result != OK) {
			const Ref kernel = memnew(ComputeShaderKernel);
			const auto entry_point_function = entry_point->getFunctionReflection();
			const String entry_point_name = entry_point_function->getName();
			kernel->set_kernel_name(entry_point_name);
			const Ref spirv = memnew(RDShaderSPIRV);
			kernel->set_spirv(spirv);
			spirv->set_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE, SlangBlob::blob_to_string(diagnostics_blob));
			return kernel;
		}
	}

	Slang::ComPtr<slang::IBlob> compiled_blob;
	{
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		const SlangResult result = linked_program->getEntryPointCode(
				0, 0, compiled_blob.writeRef(), diagnostics_blob.writeRef());
		if (result != OK) {
			compile_error = SlangBlob::blob_to_string(diagnostics_blob);
		} else if (diagnostics_blob) {
			// TODO: ignore these until we figure out:
			// (0): error 100: failed to load downstream compiler 'spirv-opt'
			// (0): note 99999: failed to load dynamic library 'slang-glslang'
			// UtilityFunctions::push_warning("Slang (link): ", SlangBlob::blob_to_string(diagnostics_blob));
		}
	}

	const Ref<SlangComponentType> component_type = create(linked_program.get(), compile_error);
	ERR_FAIL_NULL_V(component_type, nullptr);
	return component_type->compile_kernel(global_params_shape);
}
