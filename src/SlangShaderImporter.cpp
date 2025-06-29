#include "SlangShaderImporter.h"

#include "slang-com-helper.h"
#include "slang-com-ptr.h"
#include "slang.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>

#include <compute_shader_file.h>
#include <compute_shader_kernel.h>

void SlangShaderImporter::_bind_methods(){
	BIND_STATIC_METHOD(SlangShaderImporter, get_editor_setting_gen_path)
}

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
	return "tres";
}

String SlangShaderImporter::_get_resource_type() const {
	return "SlangShader";
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
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const EditorInterface *editor_interface = EditorInterface::get_singleton();
	if (!project_settings || !editor_interface) {
		return FAILED;
	}

	const String gen_dir = project_settings->get_setting(get_editor_setting_gen_path());
	const String glsl_filename = gen_dir.path_join(p_source_file.get_basename().trim_prefix("res://") + ".glsl");

	if (const auto make_dir_err = DirAccess::make_dir_recursive_absolute(project_settings->globalize_path(glsl_filename.get_base_dir()))) {
		return make_dir_err;
	}

	String glsl_source{};
	if (const Error compile_error = slang_compile_glsl(project_settings->globalize_path(p_source_file), glsl_source)) {
		return compile_error;
	}

	const Ref<FileAccess> glsl_file = FileAccess::open(glsl_filename, FileAccess::WRITE);
	if (glsl_file.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	if (!glsl_file->store_string(String("#[compute]\n%s") % glsl_source)) {
		return ERR_FILE_CANT_WRITE;
	}
	glsl_file->close();

	editor_interface->get_resource_filesystem()->update_file(glsl_filename);
	if (const Error append_error = const_cast<SlangShaderImporter *>(this)->append_import_external_resource(glsl_filename)) {
		return append_error;
	}

	const_cast<TypedArray<String> &>(p_gen_files).push_back(glsl_filename);

	const Ref slang_shader = memnew(ComputeShaderFile);
	const Ref kernel = memnew(ComputeShaderKernel);
	kernel->set_shader_file(ResourceLoader::get_singleton()->load(glsl_filename));
	TypedArray<ComputeShaderKernel> kernels;
	kernels.push_back(kernel);
	slang_shader->set_kernels(kernels);

	const String out_filename = p_save_path + String(".") + _get_save_extension();
	return ResourceSaver::get_singleton()->save(slang_shader, out_filename);
}

Error SlangShaderImporter::slang_compile_glsl(const String &p_source_file, String &out_glsl_source) {
	Slang::ComPtr<slang::IGlobalSession> globalSession;
	createGlobalSession(globalSession.writeRef());

	slang::SessionDesc sessionDesc = {};
	slang::TargetDesc targetDesc = {};
	targetDesc.format = SLANG_GLSL;
	targetDesc.profile = globalSession->findProfile("glsl_450");

	sessionDesc.targets = &targetDesc;
	sessionDesc.targetCount = 1;

	Slang::ComPtr<slang::ISession> session;
	globalSession->createSession(sessionDesc, session.writeRef());

	const Ref<FileAccess> shader_file = FileAccess::open(p_source_file, FileAccess::READ);
	if (shader_file.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	const String shader_source = shader_file->get_as_text(true);

	Slang::ComPtr<slang::IModule> slangModule;
	{
		Slang::ComPtr<slang::IBlob> diagnosticsBlob;
		slangModule = session->loadModuleFromSourceString(
				"main_module",
				p_source_file.utf8().get_data(),
				shader_source.utf8().get_data(),
				diagnosticsBlob.writeRef());
		if (!slangModule) {
			return FAILED;
		}
	}

	Slang::ComPtr<slang::IEntryPoint> entryPoint;
	{
		Slang::ComPtr<slang::IBlob> diagnosticsBlob;
		const auto entryPointFunction = "computeMain";
		slangModule->findEntryPointByName(entryPointFunction, entryPoint.writeRef());
		if (!entryPoint) {
			UtilityFunctions::push_error(String("Slang: Error getting entry point '%s'") % String(entryPointFunction));
			return FAILED;
		}
	}

	const std::array<slang::IComponentType *, 2> componentTypes = {
		slangModule,
		entryPoint
	};

	Slang::ComPtr<slang::IComponentType> composedProgram;
	{
		Slang::ComPtr<slang::IBlob> diagnosticsBlob;
		const SlangResult result = session->createCompositeComponentType(
				componentTypes.data(),
				componentTypes.size(),
				composedProgram.writeRef(),
				diagnosticsBlob.writeRef());
		if (result != OK) {
			return FAILED;
		}
	}

	Slang::ComPtr<slang::IComponentType> linkedProgram;
	{
		Slang::ComPtr<slang::IBlob> diagnosticsBlob;
		const SlangResult result = composedProgram->link(
				linkedProgram.writeRef(),
				diagnosticsBlob.writeRef());
		if (result != OK) {
			return FAILED;
		}
	}

	Slang::ComPtr<slang::IBlob> compiledBlob;
	{
		Slang::ComPtr<slang::IBlob> diagnosticsBlob;
		const SlangResult result = linkedProgram->getEntryPointCode(
			0, 0, compiledBlob.writeRef(), diagnosticsBlob.writeRef());
		if (result != OK) {
			return FAILED;
		}
	}

	out_glsl_source = String::utf8(static_cast<char const *>(compiledBlob->getBufferPointer()), compiledBlob->getBufferSize());

	return OK;
}

String SlangShaderImporter::get_editor_setting_gen_path() {
	const static String gen_path = "slang/import/generated_glsl_dir";
	return gen_path;
}
