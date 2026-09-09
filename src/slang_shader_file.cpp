#include "slang_shader_file.h"

#include "godot_cpp/classes/engine.hpp"

using namespace godot;

void SlangShaderFile::_bind_methods() {
	BIND_ENUM_CONSTANT(UNKNOWN)
	BIND_ENUM_CONSTANT(ROW_MAJOR)
	BIND_ENUM_CONSTANT(COLUMN_MAJOR)
	BIND_GET_SET_RESOURCE_ARRAY(SlangShaderFile, programs, SlangShaderProgram)
	BIND_GET_SET(SlangShaderFile, base_error, Variant::STRING)
	BIND_GET_SET_RESOURCE(SlangShaderFile, parameters, StructTypeLayoutShape);
	BIND_GET_SET_RESOURCE(SlangShaderFile, material_shader, Shader);
	BIND_GET_SET(SlangShaderFile, material_source, Variant::STRING)
	BIND_GET_SET(SlangShaderFile, material_error, Variant::STRING)
	ClassDB::bind_method(D_METHOD("set_bytecode", "bytecode", "version", "program_index"), &SlangShaderFile::set_bytecode, DEFVAL(StringName("")), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_spirv", "version", "program_index"), &SlangShaderFile::get_spirv, DEFVAL(StringName("")), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_version_list", "program_index"), &SlangShaderFile::get_version_list, DEFVAL(0));
}

void SlangShaderFile::set_bytecode(const Ref<RDShaderSPIRV>& p_bytecode, [[maybe_unused]] const StringName& p_version, const int64_t program_index) {
	ERR_FAIL_INDEX(program_index, programs.size());
	const Ref<SlangShaderProgram>& program = programs[program_index];
	ERR_FAIL_NULL(program);
	program->set_spirv(p_bytecode);
}

Ref<RDShaderSPIRV> SlangShaderFile::get_spirv([[maybe_unused]] const StringName& p_version, const int64_t program_index) const {
	ERR_FAIL_INDEX_V(program_index, programs.size(), nullptr);
	const Ref<SlangShaderProgram>& program = programs[program_index];
	ERR_FAIL_NULL_V(program, nullptr);
	return program->get_spirv();
}

TypedArray<StringName> SlangShaderFile::get_version_list(const int64_t program_index) const {
	TypedArray<StringName> version_list{};
	ERR_FAIL_INDEX_V(program_index, programs.size(), version_list);
	return version_list;
}

String SlangShaderFile::get_godot_version_string() {
	const Dictionary version_info = Engine::get_singleton()->get_version_info();
	static const auto major_version_string = String::num_int64(version_info.get("major", 0));
	static const auto minor_version_string = String::num_int64(version_info.get("minor", 0));
	static auto version_string = String("%s.%s") % Array { major_version_string, minor_version_string };
	return version_string;
}

GET_SET_PROPERTY_IMPL(SlangShaderFile, TypedArray<SlangShaderProgram>, programs)
GET_SET_PROPERTY_IMPL(SlangShaderFile, String, base_error)
GET_SET_PROPERTY_IMPL(SlangShaderFile, Ref<StructTypeLayoutShape>, parameters);
GET_SET_PROPERTY_IMPL(SlangShaderFile, Ref<Shader>, material_shader)
GET_SET_PROPERTY_IMPL(SlangShaderFile, String, material_source)
GET_SET_PROPERTY_IMPL(SlangShaderFile, String, material_error)
