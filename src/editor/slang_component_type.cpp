#include "compute_shader_cursor.h"
#include "reflection_context.h"

#include "slang_component_type.h"

using namespace godot;

void SlangComponentType::_bind_methods() {
	BIND_GET_SET(SlangComponentType, diagnostic, Variant::STRING)
	BIND_METHOD(SlangComponentType, link)
	ClassDB::bind_method(D_METHOD("compile_entry_point","entry_point_index", "target_index"), &SlangComponentType::compile_entry_point, DEFVAL(0), DEFVAL(0));
}

slang::IComponentType* SlangComponentType::get_component_type() const {
	return component_type.get();
}

slang::ProgramLayout* SlangComponentType::get_layout() const {
	slang::IComponentType* component_type_ptr = get_component_type();
	ERR_FAIL_NULL_V(component_type_ptr, nullptr);
	return component_type_ptr->getLayout();
}

Ref<StructTypeLayoutShape> SlangComponentType::get_params_shape() const {
	const SlangReflectionContext reflection_context(get_layout());
	return reflection_context.get_params_shape();
}

Variant SlangComponentType::to_json() const {
	const SlangReflectionContext reflection_context(get_layout());
	return reflection_context.to_json();
}

Ref<SlangComponentType> SlangComponentType::link() const {
	ERR_FAIL_NULL_V(component_type, nullptr);
	Slang::ComPtr<slang::IBlob> diagnostics_blob;
	slang::IComponentType* linked_component;
	ERR_FAIL_COND_V(SLANG_FAILED(component_type->link(&linked_component, diagnostics_blob.writeRef())), nullptr);
	if (diagnostics_blob) {
		return create(linked_component, SlangBlob::blob_to_string(diagnostics_blob));
	}
	return create(linked_component);
}

Ref<SlangBlob> SlangComponentType::compile_entry_point(const int64_t entry_point_index, const int64_t target_index) const {
	ERR_FAIL_NULL_V(component_type, nullptr);
	Slang::ComPtr<slang::IBlob> entry_point_blob;
	Slang::ComPtr<slang::IBlob> diagnostics_blob;
	ERR_FAIL_COND_V(SLANG_FAILED(component_type->getEntryPointCode(entry_point_index, target_index, entry_point_blob.writeRef(), diagnostics_blob.writeRef())), nullptr);
	return SlangBlob::create(entry_point_blob.get(), diagnostics_blob.get());
}

Ref<ComputeShaderKernel> SlangComponentType::compile_kernel(const Ref<ShaderTypeLayoutShape>& global_params_shape) const {
	ERR_FAIL_NULL_V(component_type, nullptr);

	slang::ProgramLayout* program_layout = get_layout();

	slang::EntryPointReflection* entry_point_layout = program_layout->getEntryPointByIndex(0);
	{
		if (entry_point_layout->getStage() != SLANG_STAGE_COMPUTE) {
			UtilityFunctions::push_warning(String("Slang: Skipping compilation of kernel '%s' (non-compute shader)") % entry_point_layout->getName());
			return nullptr;
		}
	}

	Ref kernel = memnew(ComputeShaderKernel);
	const Ref spirv = memnew(RDShaderSPIRV);
	kernel->set_spirv(spirv);

	const SlangReflectionContext reflection_context(program_layout);
	kernel->set_user_attributes(reflection_context.get_attributes(entry_point_layout->getFunction()));
	const Ref<StructTypeLayoutShape> entry_point_params_shape = reflection_context.get_entry_point_params_shape(entry_point_layout);
	kernel->set_parameters(entry_point_params_shape);

	if (slang::VariableLayoutReflection* var_layout = entry_point_layout->getVarLayout()) {
		kernel->set_space_offset(static_cast<int64_t>(var_layout->getOffset(slang::ParameterCategory::SubElementRegisterSpace)));
		kernel->set_slot_offset(static_cast<int64_t>(var_layout->getOffset(slang::ParameterCategory::DescriptorTableSlot)));
	}

	const Ref<SlangBlob> compiled_blob = compile_entry_point(0);
	ERR_FAIL_NULL_V(compiled_blob, nullptr);

	String compile_error{};
	slang::IMetadata* metadata;
	{
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		const SlangResult result = component_type->getEntryPointMetadata(0, 0, &metadata, diagnostics_blob.writeRef());
		if (SLANG_FAILED(result)) {
			compile_error = SlangBlob::blob_to_string(diagnostics_blob);
		} else if (diagnostics_blob) {
			UtilityFunctions::push_warning("Slang (metadata): ", String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()));
		} else {
			Dictionary used_binding_sets{};
			_get_used_bindings_sets(metadata, global_params_shape, entry_point_params_shape, used_binding_sets, kernel->get_space_offset(), kernel->get_slot_offset());
			kernel->set_used_binding_sets(used_binding_sets);
		}
	}

	if (compile_error.is_empty()) {
		spirv->set_stage_bytecode(RenderingDevice::SHADER_STAGE_COMPUTE, compiled_blob->get_buffer());
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

void SlangComponentType::_get_used_bindings_sets(slang::IMetadata* metadata, const Ref<ShaderTypeLayoutShape>& global_params_shape, const Ref<ShaderTypeLayoutShape>& entry_point_params_shape, Dictionary& out_used_binding_sets, int64_t kernel_space_offset, int64_t kernel_slot_offset) {
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

bool SlangComponentType::_is_location_used(slang::IMetadata* metadata, const int64_t space, const int64_t binding_index) {
	bool slot_used{};
	for (int64_t category = 0; category < SLANG_PARAMETER_CATEGORY_COUNT; category++) {
		if (SLANG_SUCCEEDED(metadata->isParameterLocationUsed(static_cast<SlangParameterCategory>(category), space, binding_index, slot_used)) && slot_used) {
			return true;
		}
	}
	return false;
}

Ref<SlangComponentType> SlangComponentType::create(slang::IComponentType* component_type, const String& diagnostic) {
	Ref ret = memnew(SlangComponentType);
	ret->component_type = component_type;
	ret->set_diagnostic(diagnostic);
	return ret;
}

GET_SET_PROPERTY_IMPL(SlangComponentType, String, diagnostic)
