#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "compute_dispatch_context.h"
#include "compute_shader_effect.h"
#include "compute_shader_file.h"
#include "compute_shader_kernel.h"
#include "compute_shader_shape.h"
#include "compute_shader_task.h"
#include "compute_texture.h"

#ifdef SLANG_IMPORT_ENABLED
#include "slang_shader_editor_plugin.h"
#include "slang_shader_importer.h"
#include "slang_entry_point.h"
#include "slang_session.h"
#include "slang_module.h"
#endif

using namespace godot;

void initialize_gdextension_types(const ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(ComputeShaderKernel);
		GDREGISTER_CLASS(ComputeShaderTask);
		GDREGISTER_ABSTRACT_CLASS(ShaderTypeLayoutShape);
		GDREGISTER_CLASS(VariantTypeLayoutShape);
		GDREGISTER_CLASS(ArrayTypeLayoutShape);
		GDREGISTER_CLASS(StructTypeLayoutShape);
		GDREGISTER_CLASS(ResourceTypeLayoutShape);
		GDREGISTER_VIRTUAL_CLASS(ComputeShaderFile);
		GDREGISTER_VIRTUAL_CLASS(CompositorEffectDispatchContext);
		GDREGISTER_VIRTUAL_CLASS(ComputeTextureDispatchContext);
		GDREGISTER_CLASS(ComputeShaderEffect);
		GDREGISTER_CLASS(ComputeTexture);
#ifdef SLANG_IMPORT_ENABLED
		GDREGISTER_CLASS(gdslang::SlangSession);
		GDREGISTER_CLASS(gdslang::SlangModule);
		GDREGISTER_CLASS(SlangEntryPoint);
#endif
	}
#if defined(SLANG_IMPORT_ENABLED) && defined(TOOLS_ENABLED)
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(SlangShaderEditorPlugin);
		GDREGISTER_CLASS(SlangShaderImporter);
		EditorPlugins::add_by_type<SlangShaderEditorPlugin>();
	}
#endif
}

void uninitialize_gdextension_types(const ModuleInitializationLevel p_level) {
#if defined(SLANG_IMPORT_ENABLED) && defined(TOOLS_ENABLED)
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::remove_by_type<SlangShaderEditorPlugin>();
	}
#endif
}

extern "C" {
GDExtensionBool GDE_EXPORT shader_slang_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
	init_obj.register_initializer(initialize_gdextension_types);
	init_obj.register_terminator(uninitialize_gdextension_types);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}