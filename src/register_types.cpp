#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "slang_shader_context.h"
#include "slang_shader_effect.h"
#include "slang_shader_file.h"
#include "slang_shader_program.h"
#include "compute_shader_shape.h"
#include "slang_shader_task.h"
#include "slang_shader_texture.h"

#ifdef SLANG_IMPORT_ENABLED
#include "slang_shader_editor_plugin.h"
#include "slang_shader_importer.h"
#include "slang_blob.h"
#include "slang_component_type.h"
#include "slang_entry_point.h"
#include "slang_session.h"
#include "slang_module.h"
#endif

using namespace godot;

void initialize_gdextension_types(const ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(SlangShaderProgram);
		GDREGISTER_CLASS(SlangShaderTask);
		GDREGISTER_ABSTRACT_CLASS(ShaderTypeLayoutShape);
		GDREGISTER_CLASS(VariantTypeLayoutShape);
		GDREGISTER_CLASS(ArrayTypeLayoutShape);
		GDREGISTER_CLASS(StructTypeLayoutShape);
		GDREGISTER_CLASS(ResourceTypeLayoutShape);
		GDREGISTER_VIRTUAL_CLASS(SlangShaderFile);
		GDREGISTER_VIRTUAL_CLASS(SlangShaderEffectContext);
		GDREGISTER_VIRTUAL_CLASS(SlangShaderTextureContext);
		GDREGISTER_CLASS(SlangShaderEffect);
		GDREGISTER_CLASS(SlangShaderTexture);
#ifdef SLANG_IMPORT_ENABLED
		GDREGISTER_ABSTRACT_CLASS(SlangComponentType);
		GDREGISTER_CLASS(gdslang::SlangSession);
		GDREGISTER_CLASS(gdslang::SlangModule);
		GDREGISTER_CLASS(SlangEntryPoint);
		GDREGISTER_CLASS(SlangBlob)
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