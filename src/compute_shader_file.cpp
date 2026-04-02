#include "compute_shader_file.h"

#include "godot_cpp/classes/engine.hpp"

using namespace godot;

void ComputeShaderFile::_bind_methods() {
	BIND_ENUM_CONSTANT(UNKNOWN)
	BIND_ENUM_CONSTANT(ROW_MAJOR)
	BIND_ENUM_CONSTANT(COLUMN_MAJOR)
	BIND_GET_SET_RESOURCE_ARRAY(ComputeShaderFile, kernels, ComputeShaderKernel)
	BIND_GET_SET(ComputeShaderFile, base_error, Variant::STRING)
	BIND_GET_SET_RESOURCE(ComputeShaderFile, parameters, StructTypeLayoutShape);
	ClassDB::bind_method(D_METHOD("set_bytecode", "bytecode", "version", "kernel_index"), &ComputeShaderFile::set_bytecode, DEFVAL(StringName("")), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_spirv", "version", "kernel_index"), &ComputeShaderFile::get_spirv, DEFVAL(StringName("")), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_version_list", "kernel_index"), &ComputeShaderFile::get_version_list, DEFVAL(0));
}

void ComputeShaderFile::set_bytecode(const Ref<RDShaderSPIRV>& p_bytecode, [[maybe_unused]] const StringName& p_version, const int64_t kernel_index) {
	ERR_FAIL_INDEX(kernel_index, kernels.size());
	const Ref<ComputeShaderKernel>& kernel = kernels[kernel_index];
	ERR_FAIL_NULL(kernel);
	kernel->set_spirv(p_bytecode);
}

Ref<RDShaderSPIRV> ComputeShaderFile::get_spirv([[maybe_unused]] const StringName& p_version, const int64_t kernel_index) const {
	ERR_FAIL_INDEX_V(kernel_index, kernels.size(), nullptr);
	const Ref<ComputeShaderKernel>& kernel = kernels[kernel_index];
	ERR_FAIL_NULL_V(kernel, nullptr);
	return kernel->get_spirv();
}

TypedArray<StringName> ComputeShaderFile::get_version_list(const int64_t kernel_index) const {
	TypedArray<StringName> version_list{};
	ERR_FAIL_INDEX_V(kernel_index, kernels.size(), version_list);
	return version_list;
}

String ComputeShaderFile::get_godot_version_string() {
	const Dictionary version_info = Engine::get_singleton()->get_version_info();
	static const auto major_version_string = String::num_int64(version_info.get("major", 0));
	static const auto minor_version_string = String::num_int64(version_info.get("minor", 0));
	static auto version_string = String("%s.%s") % Array { major_version_string, minor_version_string };
	return version_string;
}

GET_SET_PROPERTY_IMPL(ComputeShaderFile, TypedArray<ComputeShaderKernel>, kernels)
GET_SET_PROPERTY_IMPL(ComputeShaderFile, String, base_error)
GET_SET_PROPERTY_IMPL(ComputeShaderFile, Ref<StructTypeLayoutShape>, parameters);
