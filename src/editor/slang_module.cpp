#include "slang_module.h"

#include "compute_shader_cursor.h"
#include "compute_shader_file.h"
#include "reflection_context.h"

using namespace gdslang;
using namespace godot;

void SlangModule::_bind_methods() {
	BIND_GET_SET(SlangModule, diagnostic, Variant::STRING);
	ClassDB::bind_method(D_METHOD("compile_kernels", "additional_entry_points"), &SlangModule::compile_kernels, DEFVAL(PackedStringArray{}));
	ClassDB::bind_method(D_METHOD("compile_shader", "additional_entry_points"), &SlangModule::compile_shader, DEFVAL(PackedStringArray{}));
	BIND_METHOD(SlangModule, get_params_shape)
	BIND_METHOD(SlangModule, to_json)
}

slang::IModule* SlangModule::get_module() const {
	return module.get();
}

void SlangModule::set_module(slang::IModule* p_module) {
	module = p_module;
}

slang::ProgramLayout* SlangModule::get_layout() const {
	ERR_FAIL_NULL_V(module, nullptr);
	return module->getLayout();
}

String SlangModule::get_file_path() const {
	ERR_FAIL_NULL_V(module, {});
	return module->getFilePath();
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
			UtilityFunctions::push_error(String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()));
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
					spirv->set_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE, String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()).trim_suffix("\n"));
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
	String slang_error{};

	if (!diagnostic.is_empty()) {
		slang_error = diagnostic;
	}

	const Ref slang_shader = memnew(ComputeShaderFile);
	if (slang_error.is_empty()) {
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
		slang_shader->set_base_error(slang_error);
	}

	slang_shader->set_meta("godot_version", ComputeShaderFile::get_godot_version_string());
	return slang_shader;
}

Ref<StructTypeLayoutShape> SlangModule::get_params_shape() const {
	const SlangReflectionContext reflection_context(get_layout());
	return reflection_context.get_params_shape();
}

Variant SlangModule::to_json() const {
	const SlangReflectionContext reflection_context(get_layout());
	return reflection_context.to_json();
}

Ref<ComputeShaderKernel> SlangModule::_compile_kernel(slang::IEntryPoint* entry_point, const Ref<ShaderTypeLayoutShape>& global_params_shape) {
	ERR_FAIL_NULL_V(module, nullptr);
	slang::ISession* session = module->getSession();

	const Ref kernel = memnew(ComputeShaderKernel);

	const auto entry_point_function = entry_point->getFunctionReflection();
	const String entry_point_name = entry_point_function->getName();
	kernel->set_kernel_name(entry_point_name);

	const Ref spirv = memnew(RDShaderSPIRV);
	kernel->set_spirv(spirv);

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

	const SlangReflectionContext reflection_context(program_layout);
	kernel->set_user_attributes(reflection_context.get_attributes(entry_point_function));
	const Ref<StructTypeLayoutShape> entry_point_params_shape = reflection_context.get_entry_point_params_shape(entry_point_layout);
	kernel->set_parameters(entry_point_params_shape);

	if (slang::VariableLayoutReflection* var_layout = entry_point_layout->getVarLayout()) {
		kernel->set_space_offset(static_cast<int64_t>(var_layout->getOffset(slang::ParameterCategory::SubElementRegisterSpace)));
		kernel->set_slot_offset(static_cast<int64_t>(var_layout->getOffset(slang::ParameterCategory::DescriptorTableSlot)));
	}

	slang::IMetadata* metadata;
	{
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		const SlangResult result = linked_program->getEntryPointMetadata(0, 0, &metadata, diagnostics_blob.writeRef());
		if (SLANG_FAILED(result)) {
			compile_error = String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize());
		} else if (diagnostics_blob) {
			UtilityFunctions::push_warning("Slang (metadata): ", String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()));
		} else {
			Dictionary used_binding_sets{};
			_get_used_bindings_sets(metadata, global_params_shape, entry_point_params_shape, used_binding_sets, kernel->get_space_offset(), kernel->get_slot_offset());
			kernel->set_used_binding_sets(used_binding_sets);
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

	return kernel;
}

void SlangModule::_get_used_bindings_sets(slang::IMetadata* metadata, const Ref<ShaderTypeLayoutShape>& global_params_shape, const Ref<ShaderTypeLayoutShape>& entry_point_params_shape, Dictionary& out_used_binding_sets, int64_t kernel_space_offset, int64_t kernel_slot_offset) {
	ERR_FAIL_NULL(metadata);
	ComputeShaderObject global_object(nullptr, nullptr, global_params_shape);
	ComputeShaderObject entry_point_object(nullptr, nullptr, entry_point_params_shape, kernel_space_offset, kernel_slot_offset);
	ComputeShaderObject::DescriptorSets descriptor_sets;
	uint64_t next_space_index = 0;
	const uint64_t active_space_index = global_object.get_descriptor_sets(descriptor_sets, next_space_index);
	entry_point_object.get_descriptor_sets(descriptor_sets, active_space_index, next_space_index);
	for (const auto& [space_index, uniforms] : descriptor_sets) {
		bool set_used = out_used_binding_sets.get(space_index, false);
		if (set_used)
			continue;
		for (const Ref<RDUniform> uniform : uniforms) {
			if (uniform.is_valid()) {
				if (_is_location_used(metadata, space_index, uniform->get_binding())) {
					set_used = true;
					break;
				}
			}
		}
		out_used_binding_sets.set(space_index, set_used);
	}
}

bool SlangModule::_is_location_used(slang::IMetadata* metadata, const int64_t space, const int64_t binding_index) {
	bool slot_used{};
	for (int64_t category = 0; category < SLANG_PARAMETER_CATEGORY_COUNT; category++) {
		if (SLANG_SUCCEEDED(metadata->isParameterLocationUsed(static_cast<SlangParameterCategory>(category), space, binding_index, slot_used)) && slot_used) {
			return true;
		}
	}
	return false;
}

GET_SET_PROPERTY_IMPL(SlangModule, String, diagnostic)
