#include "godot_cpp/classes/editor_file_system.hpp"
#include "godot_cpp/classes/editor_interface.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/window.hpp"

#include "compute_shader_task.h"

#include "attributes.h"
#include "compute_shader_cursor.h"
#include "compute_shader_shape.h"

using namespace godot;

void ComputeShaderTask::_bind_methods() {
	BIND_GET_SET_RESOURCE(ComputeShaderTask, shader, ComputeShaderFile)
	BIND_METHOD(ComputeShaderTask, get_shader_parameter, "param")
	BIND_METHOD(ComputeShaderTask, set_shader_parameter, "param", "value")
	BIND_METHOD(ComputeShaderTask, clear_shader_parameters)
	ClassDB::bind_method(D_METHOD("dispatch","kernel_name", "thread_groups", "context"), &ComputeShaderTask::dispatch, DEFVAL(nullptr));
	ClassDB::bind_method(D_METHOD("dispatch_at","kernel_index", "thread_groups", "context"), &ComputeShaderTask::dispatch_at, DEFVAL(nullptr));
	ClassDB::bind_method(D_METHOD("dispatch_all","thread_groups", "context"), &ComputeShaderTask::dispatch_all, DEFVAL(nullptr));
	ClassDB::bind_method(D_METHOD("dispatch_group","group_name", "thread_groups", "context"), &ComputeShaderTask::dispatch_group, DEFVAL(nullptr));
}

ComputeShaderTask::ComputeShaderTask() : _shader_object(nullptr) { }

TypedArray<ComputeShaderKernel> ComputeShaderTask::get_kernels() const {
	if (shader.is_valid()) {
		return shader->get_kernels().duplicate();
	}
	return {};
}

Ref<ComputeShaderFile> ComputeShaderTask::get_shader() const { return shader; }

void ComputeShaderTask::set_shader(Ref<ComputeShaderFile> p_shader) {
	if (shader != p_shader) {
		const Callable changed_callable = callable_mp(this, &ComputeShaderTask::_shader_changed);
		if (shader.is_valid() && shader->is_connected("changed", changed_callable)) {
			shader->disconnect("changed", changed_callable);
		}
		shader = p_shader;
		if (p_shader.is_valid()) {
			p_shader->connect("changed", changed_callable);
		}
		RenderingServer::get_singleton()->call_on_render_thread(changed_callable);
		emit_changed();
	}
}

Variant ComputeShaderTask::get_shader_parameter(const StringName& param) const {
	const PackedStringArray parts = param.split("/");
	Variant current = _shader_parameters;
	int64_t i = 0;
	bool valid;
	for (; i < parts.size() - 1; ++i) {
		current = current.get_named(parts[i], valid);
		if (!valid || current.get_type() == Variant::NIL) return nullptr;
	}
	return current.get_named(parts[i], valid);
}

void ComputeShaderTask::set_shader_parameter(const StringName& param, const Variant& value) {
	const PackedStringArray parts = param.split("/");
	Variant current = _shader_parameters;
	int64_t i = 0;
	bool valid;
	for (; i < parts.size() - 1; ++i) {
		Variant next = current.get_named(parts[i], valid);
		if (!valid || next.get_type() == Variant::NIL) {
			next = Dictionary();
			current.set_named(parts[i], next, valid);
		}
		current = next;
	}
	current.set_named(parts[i], value, valid);
}

void ComputeShaderTask::clear_shader_parameters() {
	_shader_parameters.clear();
}

void ComputeShaderTask::dispatch_all(const Vector3i thread_groups, const Object* context) {
	ERR_FAIL_NULL(shader);
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	for (int64_t i = 0; i < kernels.size(); i++) {
		_dispatch(i, thread_groups, context);
	}
}

void ComputeShaderTask::dispatch(const StringName& kernel_name, const Vector3i thread_groups, const Object* context) {
	ERR_FAIL_NULL(shader);
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	for (int64_t i = 0; i < kernels.size(); i++) {
		Ref<ComputeShaderKernel> kernel = kernels[i];
		if (kernel.is_valid() && kernel->get_kernel_name() == kernel_name) {
			_dispatch(i, thread_groups, context);
		}
	}
}

void ComputeShaderTask::dispatch_at(const int64_t kernel_index, const Vector3i thread_groups, const Object* context) {
	_dispatch(kernel_index, thread_groups, context);
}

void ComputeShaderTask::dispatch_group(const StringName& group_name, const Vector3i thread_groups, const Object* context) {
	ERR_FAIL_NULL(shader);
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	for (int64_t i = 0; i < kernels.size(); i++) {
		const Ref<ComputeShaderKernel> kernel = kernels[i];
		if (kernel.is_valid()) {
			const Dictionary attributes = kernel->get_user_attributes();
			if (attributes.has(GodotAttributes::kernel_group())) {
				const Dictionary kernel_group_attr = attributes[GodotAttributes::kernel_group()];
				if (kernel_group_attr["group_name"] == group_name) {
					_dispatch(i, thread_groups, context);
				}
			}
		}
	}
}

Dictionary ComputeShaderTask::get_shader_parameters() const {
	if (shader.is_null()) {
		return {};
	}
	const Ref<StructTypeLayoutShape> params_shape = shader->get_parameters();
	if (params_shape.is_null()) {
		return {};
	}
	return params_shape->get_properties().duplicate();
}

bool ComputeShaderTask::_set(const StringName& p_name, const Variant& p_value) {
	if (p_name.begins_with("shader_parameter/")) {
		const StringName param_name = p_name.substr(17);
		set_shader_parameter(param_name, p_value);
		return true;
	}
	return false;
}

bool ComputeShaderTask::_get(const StringName& p_name, Variant& r_ret) const {
	if (p_name.begins_with("shader_parameter/")) {
		const StringName param_name = p_name.substr(17);
		const Dictionary params = get_shader_parameters();
		Dictionary reflection;
		if (_property_get_reflection(p_name, reflection)) {
			const Variant value = get_shader_parameter(param_name);
			if (value.get_type() == Variant::NIL && reflection.has("default_value")) {
				r_ret = reflection["default_value"];
				return true;
			}
			if (reflection.has("property_info")) {
				const PropertyInfo property_info = PropertyInfo::from_dict(reflection["property_info"]);
				r_ret = UtilityFunctions::type_convert(value, property_info.type);
				return true;
			}
			r_ret = value;
			return true;
		}
	}
	return false;
}

void ComputeShaderTask::_get_property_list(List<PropertyInfo>* p_list) const {
	ERR_FAIL_NULL(p_list);
	_get_property_list(p_list, "shader_parameter/", get_shader_parameters());
}

void ComputeShaderTask::_get_property_list(List<PropertyInfo>* p_list, const String& prefix, const Dictionary& properties) {
	ERR_FAIL_NULL(p_list);
	for (const StringName property_name : properties.keys()) {
		const Dictionary property = properties.get(property_name, Dictionary());
		PropertyInfo property_info = PropertyInfo::from_dict(property.get("property_info", Dictionary()));
		property_info.name = prefix + property_info.name;
		if (_can_show_property_info(property_info)) {
			p_list->push_back(property_info);
		} else {
			const Ref<ShaderTypeLayoutShape> property_shape = property.get("shape", nullptr);
			if (const auto structured_shape = cast_to<StructTypeLayoutShape>(property_shape.ptr())) {
				_get_property_list(p_list, String("%s%s/") % TypedArray<String> { prefix, property_name }, structured_shape->get_properties());
			}
		}
	}
}

bool ComputeShaderTask::_can_show_property_info(const PropertyInfo& property_info) {
	return property_info.type != Variant::NIL
		&& property_info.type != Variant::RID
		&& (property_info.type != Variant::OBJECT || property_info.hint == PROPERTY_HINT_RESOURCE_TYPE);
}

bool ComputeShaderTask::_property_can_revert(const StringName& p_name) const {
	Dictionary reflection;
	return _property_get_reflection(p_name, reflection);
}

bool ComputeShaderTask::_property_get_revert(const StringName& p_name, Variant& r_property) const {
	Dictionary reflection;
	if (_property_get_reflection(p_name, reflection) && reflection.has("default_value")) {
		r_property = reflection["default_value"];
		return true;
	}
	return false;
}

bool ComputeShaderTask::_property_get_reflection(const StringName& p_name, Dictionary& r_reflection) const {
	if (p_name.begins_with("shader_parameter/")) {
		const PackedStringArray parts = p_name.substr(17).split("/");
		Variant current = get_shader_parameters();
		int64_t i = 0;
		bool valid;
		for (; i < parts.size() - 1; ++i) {
			const Dictionary property = current.get_named(parts[i], valid);
			const Ref<ShaderTypeLayoutShape> shape = property.get("shape", nullptr);
			const auto structured_shape = cast_to<StructTypeLayoutShape>(shape.ptr());
			if (!valid || !structured_shape) return false;
			current = structured_shape->get_properties();
		}
		r_reflection = current.get_named(parts[i], valid);
		return valid;
	}
	return false;
}

void ComputeShaderTask::_reset() {
	const RenderingServer* rendering_server = RenderingServer::get_singleton();
	ERR_FAIL_NULL_MSG(rendering_server, "ComputeShaderTask: Couldn't obtain RenderingServer for dispatch!");
	RenderingDevice* rendering_device = rendering_server->get_rendering_device();
	ERR_FAIL_NULL_MSG(rendering_device, "ComputeShaderTask: Couldn't obtain rendering device for dispatch!");

	for (const Variant& key : _kernel_pipelines.keys()) {
		const RID rid = _kernel_pipelines[key];
		if (rid.is_valid()) {
			rendering_device->free_rid(_kernel_pipelines[key]);
		}
	}
	_kernel_pipelines.clear();

	for (const Variant& key : _kernel_shaders.keys()) {
		const RID rid = _kernel_shaders[key];
		if (rid.is_valid()) {
			rendering_device->free_rid(_kernel_shaders[key]);
		}
	}
	_kernel_shaders.clear();

	if (!_sampler_cache) {
		_sampler_cache = std::make_unique<SamplerCache>(rendering_device);
	}
	if (shader.is_valid() && shader->get_base_error().is_empty()) {
		_shader_object = std::make_unique<ComputeShaderObject>(rendering_device, _sampler_cache.get(), shader->get_parameters());
	} else {
		_shader_object = nullptr;
	}
}

void ComputeShaderTask::_shader_changed() {
	_reset();
	notify_property_list_changed();
}

RID ComputeShaderTask::_get_shader_rid(const int64_t kernel_index, RenderingDevice* rd) {
	ERR_FAIL_NULL_V(shader, {});
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	if (!_kernel_shaders.has(kernel_index)) {
		const Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
		_kernel_shaders.set(kernel_index, rd->shader_create_from_spirv(kernel->get_spirv(), shader->get_name().get_file().trim_suffix(".slang")));
	}
	return _kernel_shaders.get(kernel_index, RID{});
}

RID ComputeShaderTask::_get_shader_pipeline_rid(const int64_t kernel_index, RenderingDevice* rd) {
	if (!_kernel_pipelines.has(kernel_index)) {
		const RID shader_rid = _get_shader_rid(kernel_index, rd);
		_kernel_pipelines.set(kernel_index, rd->compute_pipeline_create(shader_rid));
	}
	return _kernel_pipelines.get(kernel_index, RID{});
}

void ComputeShaderTask::_dispatch(const int64_t kernel_index, const Vector3i thread_groups, const Object* context) {
	ERR_FAIL_NULL(shader);
	ERR_FAIL_NULL(_shader_object);
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	ERR_FAIL_INDEX_MSG(kernel_index, kernels.size(), String("Attempted to dispatch invalid kernel index %s (max %s)!") % PackedStringArray({ String::num_int64(kernel_index), String::num_int64(kernels.size() - 1) }));

	const Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
	ERR_FAIL_NULL_MSG(kernel, String("Attempted to dispatch invalid kernel index %s (found: nil)!") % String::num_int64(kernel_index));
	ERR_FAIL_COND_MSG(!kernel->get_compile_error().is_empty(), "Can't dispatch kernel with compile error!");

	const RenderingServer* rendering_server = RenderingServer::get_singleton();
	ERR_FAIL_NULL_MSG(rendering_server, "ComputeShaderTask: Couldn't obtain RenderingServer for dispatch!");
	RenderingDevice* rendering_device = rendering_server->get_rendering_device();
	ERR_FAIL_NULL_MSG(rendering_device, "ComputeShaderTask: Couldn't obtain rendering device for dispatch!");

	ComputeShaderCursor(_shader_object.get(), context).write(_shader_parameters);
	_shader_object->flush_buffers();
	const int64_t compute_list = rendering_device->compute_list_begin();
	const RID pipeline = _get_shader_pipeline_rid(kernel_index, rendering_device);
	rendering_device->compute_list_bind_compute_pipeline(compute_list, pipeline);

	_shader_object->bind_uniforms(compute_list, _get_shader_rid(kernel_index, rendering_device));
	rendering_device->compute_list_dispatch(compute_list, thread_groups.x, thread_groups.y, thread_groups.z);
	rendering_device->compute_list_end();
}
