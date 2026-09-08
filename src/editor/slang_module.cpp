#include "godot_cpp/classes/project_settings.hpp"

#include "compute_shader_cursor.h"
#include "slang_shader_file.h"
#include "reflection_context.h"
#include "slang_entry_point.h"

#include "slang_session.h"
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
	return module;
}

slang::IModule** SlangModule::get_write_ref() {
	return &module;
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

SlangStage SlangModule::_get_entry_point_stage(slang::IEntryPoint* entry_point) {
	ERR_FAIL_NULL_V(entry_point, SLANG_STAGE_NONE);
	// an entry point's stage only shows up once it has a layout of its own
	slang::ProgramLayout* entry_point_layout = entry_point->getLayout();
	if (entry_point_layout == nullptr || entry_point_layout->getEntryPointCount() == 0) {
		return SLANG_STAGE_NONE;
	}
	return entry_point_layout->getEntryPointByIndex(0)->getStage();
}

Ref<SlangShaderProgram> SlangModule::_make_error_program(const String& program_name, const RenderingDevice::ShaderStage stage, const String& compile_error) {
	Ref<SlangShaderProgram> program;
	program.instantiate();
	Ref<RDShaderSPIRV> spirv;
	spirv.instantiate();
	spirv->set_stage_compile_error(stage, compile_error.trim_suffix("\n"));
	program->set_kernel_name(program_name);
	program->set_spirv(spirv);
	return program;
}

Error SlangModule::_compile_programs(TypedArray<Ref<SlangShaderProgram>>& out_kernels, TypedArray<Ref<SlangShaderProgram>>& out_passes, const Ref<ShaderTypeLayoutShape>& global_params_shape, const PackedStringArray& additional_entry_points) {
	ERR_FAIL_NULL_V(module, ERR_UNCONFIGURED);
	std::vector<Slang::ComPtr<slang::IEntryPoint>> compute_entry_points{};
	std::vector<Slang::ComPtr<slang::IEntryPoint>> vertex_entry_points{};
	std::vector<Slang::ComPtr<slang::IEntryPoint>> fragment_entry_points{};
	compute_entry_points.reserve(module->getDefinedEntryPointCount() + additional_entry_points.size());
	if (module->getDefinedEntryPointCount() == 0 && additional_entry_points.is_empty()) {
		Slang::ComPtr<slang::IEntryPoint> entry_point;
		Slang::ComPtr<slang::IBlob> diagnostics_blob;
		if (SLANG_SUCCEEDED(module->findAndCheckEntryPoint("main", SlangStage::SLANG_STAGE_COMPUTE, entry_point.writeRef(), diagnostics_blob.writeRef()))) {
			compute_entry_points.push_back(entry_point);
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
			switch (_get_entry_point_stage(entry_point)) {
				case SLANG_STAGE_COMPUTE:
					compute_entry_points.push_back(entry_point);
					break;
				case SLANG_STAGE_VERTEX:
					vertex_entry_points.push_back(entry_point);
					break;
				case SLANG_STAGE_FRAGMENT:
					fragment_entry_points.push_back(entry_point);
					break;
				default:
					UtilityFunctions::push_warning(String("[%s] Slang: Skipping entry point '%s' (unsupported shader stage)") % Array({ get_file_path(), String(entry_point->getFunctionReflection()->getName()) }));
					break;
			}
		}
		for (const String& entry_point_name : additional_entry_points) {
			const CharString name_string = entry_point_name.utf8();
			Slang::ComPtr<slang::IEntryPoint> entry_point;
			Slang::ComPtr<slang::IBlob> diagnostics_blob;
			if (SLANG_SUCCEEDED(module->findAndCheckEntryPoint(name_string.get_data(), SlangStage::SLANG_STAGE_COMPUTE, entry_point.writeRef(), diagnostics_blob.writeRef()))) {
				compute_entry_points.push_back(entry_point);
			} else {
				const String compile_error = diagnostics_blob && diagnostics_blob->getBufferSize() > 0
						? SlangBlob::blob_to_string(diagnostics_blob)
						: String("Failed to find entry point '%s'!") % entry_point_name;
				out_kernels.push_back(_make_error_program(entry_point_name, RenderingDevice::SHADER_STAGE_COMPUTE, compile_error));
			}
		}
	}
	for (const Slang::ComPtr<slang::IEntryPoint>& entry_point : compute_entry_points) {
		const Ref<SlangShaderProgram> kernel = _compile_kernel(entry_point, global_params_shape);
		if (kernel.is_valid()) {
			out_kernels.push_back(kernel);
		}
	}

	if (!vertex_entry_points.empty() || !fragment_entry_points.empty()) {
		// one vertex plus one fragment entry point make up the file's raster pass, which is
		// as much as a .glsl file can describe today
		if (vertex_entry_points.size() == 1 && fragment_entry_points.size() == 1) {
			const Ref<SlangShaderProgram> pass = _compile_pass(vertex_entry_points.front(), fragment_entry_points.front());
			if (pass.is_valid()) {
				out_passes.push_back(pass);
			}
		} else {
			out_passes.push_back(_make_error_program(
					get_file_path().get_file(),
					RenderingDevice::SHADER_STAGE_VERTEX,
					String("Expected exactly one vertex and one fragment entry point, found %s and %s!") % Array({ String::num_int64(vertex_entry_points.size()), String::num_int64(fragment_entry_points.size()) })));
		}
	}

	return OK;
}

Ref<SlangShaderFile> SlangModule::compile_shader(const PackedStringArray& additional_entry_points) {
	const Ref slang_shader = memnew(SlangShaderFile);
	const String diagnostic = get_diagnostic();
	if (diagnostic.is_empty()) {
		const Ref<StructTypeLayoutShape> global_params = get_params_shape();
		slang_shader->set_parameters(global_params);
		TypedArray<Ref<SlangShaderProgram>> kernels;
		TypedArray<Ref<SlangShaderProgram>> passes;
		if (const Error compile_error = _compile_programs(kernels, passes, global_params.ptr(), additional_entry_points)) {
			slang_shader->set_base_error(UtilityFunctions::error_string(compile_error));
		} else if (kernels.is_empty() && passes.is_empty()) {
			slang_shader->set_base_error("No entry points found!");
		} else {
			slang_shader->set_kernels(kernels);
			slang_shader->set_passes(passes);
		}
	} else {
		slang_shader->set_base_error(diagnostic);
	}

	slang_shader->set_meta("godot_version", SlangShaderFile::get_godot_version_string());
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
	entry_point->set_session(get_session());
	return entry_point;
}

Ref<SlangEntryPoint> SlangModule::find_entry_point(const String& name) const {
	ERR_FAIL_NULL_V(module, nullptr);
	Ref entry_point = memnew(SlangEntryPoint);
	Slang::ComPtr<slang::IBlob> diagnostics_blob;
	const CharString entry_point_name = name.utf8();
	ERR_FAIL_COND_V(SLANG_FAILED(module->findEntryPointByName(entry_point_name.get_data(), entry_point->write_ref())), nullptr);
	entry_point->set_session(get_session());
	return entry_point;
}

Ref<SlangEntryPoint> SlangModule::find_and_check_entry_point(const String& name, const RenderingDevice::ShaderStage shader_stage) const {
	ERR_FAIL_NULL_V(module, nullptr);
	const std::optional<SlangStage> slang_stage = to_slang_stage(shader_stage);
	ERR_FAIL_COND_V(!slang_stage.has_value(), nullptr);
	Ref entry_point = memnew(SlangEntryPoint);
	Slang::ComPtr<slang::IBlob> diagnostics_blob;
	const CharString entry_point_name = name.utf8();
	ERR_FAIL_COND_V(SLANG_FAILED(module->findAndCheckEntryPoint(entry_point_name.get_data(), *slang_stage, entry_point->write_ref(), diagnostics_blob.writeRef())), nullptr);
	if (diagnostics_blob) {
		entry_point->set_diagnostic(SlangBlob::blob_to_string(diagnostics_blob));
	}
	entry_point->set_session(get_session());
	return entry_point;
}

Ref<SlangShaderProgram> SlangModule::_compile_pass(slang::IEntryPoint* vertex_entry_point, slang::IEntryPoint* fragment_entry_point) {
	ERR_FAIL_NULL_V(module, nullptr);
	slang::ISession* session = module->getSession();

	// the fragment entry point names the pass, the way the compute entry point names a kernel
	const String pass_name = fragment_entry_point->getFunctionReflection()->getName();

	String compile_error{};
	Slang::ComPtr<slang::IComponentType> composed_program;
	{
		// both stages have to be composed and linked together: compiled apart they would get
		// independent binding layouts and Godot could not merge them into one shader
		const std::array<slang::IComponentType*, 3> componentTypes = {
			module,
			vertex_entry_point,
			fragment_entry_point,
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
			compile_error = SlangBlob::blob_to_string(diagnostics_blob);
		}
	}

	if (linked_program.get() == nullptr) {
		return _make_error_program(pass_name, RenderingDevice::SHADER_STAGE_VERTEX, compile_error);
	}

	const Ref<SlangComponentType> component_type = create(linked_program.get(), compile_error);
	ERR_FAIL_NULL_V(component_type, nullptr);
	component_type->set_session(get_session());
	const Ref<SlangShaderProgram> pass = component_type->compile_pass();
	if (pass.is_valid()) {
		pass->set_kernel_name(pass_name);
	}
	return pass;
}

Ref<SlangShaderProgram> SlangModule::_compile_kernel(slang::IEntryPoint* entry_point, const Ref<ShaderTypeLayoutShape>& global_params_shape) {
	ERR_FAIL_NULL_V(module, nullptr);
	slang::ISession* session = module->getSession();

	String compile_error{};
	Slang::ComPtr<slang::IComponentType> composed_program;
	{
		const std::array<slang::IComponentType*, 2> componentTypes = {
			module,
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
			const Ref kernel = memnew(SlangShaderProgram);
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
	component_type->set_session(get_session());
	return component_type->compile_kernel(global_params_shape);
}
