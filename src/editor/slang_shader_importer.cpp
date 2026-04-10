#include "slang_shader_importer.h"

#include <vector>

#include "slang-com-ptr.h"
#include "slang.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_saver.hpp>

#include "attributes.h"
#include <compute_shader_file.h>
#include <compute_shader_kernel.h>

#include "compute_shader_cursor.h"
#include "reflection_context.h"
#include "slang_session.h"
#include "slang_shader_editor_plugin.h"

using namespace godot;

void SlangShaderImporter::_bind_methods() {
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
		matrix_layout_option.set("name", "default_matrix_layout");
		matrix_layout_option.set("default_value", ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR);
		matrix_layout_option.set("property_hint", PROPERTY_HINT_ENUM);
		matrix_layout_option.set("hint_string", String("Row-major:%s,Column-major:%s") % Array { String::num_int64(ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR), String::num_int64(ShaderTypeLayoutShape::MatrixLayout::COLUMN_MAJOR) });
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
	const Ref<FileAccess> shader_file = FileAccess::open(p_source_file, FileAccess::READ);
	if (shader_file.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	const String shader_source = shader_file->get_as_text(true);

	const Ref slang_session = memnew(gdslang::SlangSession);
	slang_session->set_enable_glsl(p_source_file.ends_with(".glsl"));
	const int64_t default_matrix_layout = p_options["default_matrix_layout"];
	if (default_matrix_layout >= SLANG_MATRIX_LAYOUT_ROW_MAJOR && default_matrix_layout <= SLANG_MATRIX_LAYOUT_COLUMN_MAJOR) {
		slang_session->set_default_matrix_layout(static_cast<ShaderTypeLayoutShape::MatrixLayout>(default_matrix_layout));
	}

	const slang::ISession* session = slang_session->get_or_create_session();
	ERR_FAIL_NULL_V(session, FAILED);

	String slang_error{};

	const Ref<gdslang::SlangModule> module = slang_session->load_module_from_source_string("__main_module", p_source_file.get_file(), shader_source);
	if (!module->get_diagnostic().is_empty()) {
		slang_error = module->get_diagnostic();
	}

	const Ref slang_shader = memnew(ComputeShaderFile);
	if (module.is_valid() && slang_error.is_empty()) {
		const Ref<StructTypeLayoutShape> global_params = module->get_params_shape();
		slang_shader->set_parameters(global_params);
		TypedArray<ComputeShaderKernel> kernels;
		if (const Error compile_error = _slang_compile_kernels(module->get_module(), kernels, p_options.get("entry_points", {}), global_params.ptr())) {
			ERR_FAIL_V_MSG(compile_error, "Failed to compile Slang shader!");
		}
		slang_shader->set_kernels(kernels);
		if (kernels.is_empty()) {
			const String err_message = "No entry points found!";
			UtilityFunctions::push_error(String("[%s] ") % p_source_file, err_message);
			slang_shader->set_base_error(err_message);
		}
		for (const Ref<ComputeShaderKernel> kernel : kernels) {
			const String compile_error = kernel->get_compile_error().trim_suffix("\n");
			if (!compile_error.is_empty()) {
				UtilityFunctions::push_error(String("[%s] Slang compile error:\n%s") % Array({ module->get_file_path(), compile_error }));
			}
		}
	} else {
		UtilityFunctions::push_error(String("[%s] ") % p_source_file, String("Failed to compile Slang shader:\n%s") % slang_error);
		slang_shader->set_base_error(slang_error);
	}

	slang_shader->set_meta("godot_version", ComputeShaderFile::get_godot_version_string());

	const String out_filename = p_save_path + String(".") + _get_save_extension();
	return ResourceSaver::get_singleton()->save(slang_shader, out_filename);
}

Error SlangShaderImporter::_slang_compile_kernels(slang::IModule* slang_module, TypedArray<ComputeShaderKernel>& out_kernels, const PackedStringArray& additional_entry_points, const Ref<ShaderTypeLayoutShape>& global_params_shape) {
	ERR_FAIL_NULL_V(slang_module, ERR_BUG);
	std::vector<Slang::ComPtr<slang::IEntryPoint>> entry_points{};
	entry_points.reserve(slang_module->getDefinedEntryPointCount() + additional_entry_points.size());
	if (slang_module->getDefinedEntryPointCount() == 0 && additional_entry_points.is_empty()) {
		Slang::ComPtr<slang::IEntryPoint> entry_point;
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		if (SLANG_SUCCEEDED(slang_module->findAndCheckEntryPoint("main", SlangStage::SLANG_STAGE_COMPUTE, entry_point.writeRef(), diagnostics_blob.writeRef()))) {
			entry_points.push_back(entry_point);
		} else if (diagnostics_blob) {
			UtilityFunctions::push_error(String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()));
		}
	} else {
		for (int32_t entry_point_index = 0; entry_point_index < slang_module->getDefinedEntryPointCount(); ++entry_point_index) {
			Slang::ComPtr<slang::IEntryPoint> entry_point;
			ERR_FAIL_COND_V_MSG(
				slang_module->getDefinedEntryPoint(entry_point_index, entry_point.writeRef()) != OK,
				ERR_BUG,
				String("[%s] Slang: Error getting entry point '%s'") % Array({ slang_module->getFilePath(), String::num_int64(entry_point_index) }));
			entry_points.push_back(entry_point);
		}
		for (const String& entry_point_name : additional_entry_points) {
			const CharString name_string = entry_point_name.utf8();
			Slang::ComPtr<slang::IEntryPoint> entry_point;
			Slang::ComPtr<slang::IBlob> diagnostics_blob;
			if (SLANG_SUCCEEDED(slang_module->findAndCheckEntryPoint(name_string.get_data(), SlangStage::SLANG_STAGE_COMPUTE, entry_point.writeRef(), diagnostics_blob.writeRef()))) {
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
		const Ref<ComputeShaderKernel> kernel = _slang_compile_kernel(slang_module->getSession(), slang_module, entry_point, global_params_shape);
		if (kernel.is_valid()) {
			out_kernels.push_back(kernel);
		}
	}

	return OK;
}

Ref<ComputeShaderKernel> SlangShaderImporter::_slang_compile_kernel(slang::ISession* session, slang::IModule* slang_module, slang::IEntryPoint* entry_point, const Ref<ShaderTypeLayoutShape>& global_params_shape) {
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

void SlangShaderImporter::_get_used_bindings_sets(slang::IMetadata* metadata, const Ref<ShaderTypeLayoutShape>& global_params_shape, const Ref<ShaderTypeLayoutShape>& entry_point_params_shape, Dictionary& out_used_binding_sets, const int64_t kernel_space_offset, const int64_t kernel_slot_offset) {
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

bool SlangShaderImporter::_is_location_used(slang::IMetadata* metadata, const int64_t space, const int64_t binding_index) {
	bool slot_used{};
	for (int64_t category = 0; category < SLANG_PARAMETER_CATEGORY_COUNT; category++) {
		if (SLANG_SUCCEEDED(metadata->isParameterLocationUsed(static_cast<SlangParameterCategory>(category), space, binding_index, slot_used)) && slot_used) {
			return true;
		}
	}
	return false;
}
