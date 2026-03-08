#include "godot_cpp/classes/editor_file_system.hpp"
#include "godot_cpp/classes/editor_interface.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/uniform_set_cache_rd.hpp"
#include "godot_cpp/classes/window.hpp"

#include "attributes.h"
#include "compute_shader_cursor.h"
#include "compute_shader_shape.h"
#include "sampler_cache.h"

#include "compute_shader_task.h"

using namespace godot;

constexpr auto shader_param_prefix_chars = "shader_parameter/";
constexpr int64_t shader_param_prefix_length = std::char_traits<char>::length(shader_param_prefix_chars);

const StringName& shader_param_prefix() {
	static const StringName prefix(shader_param_prefix_chars);
	return prefix;
}

void ComputeShaderTask::_bind_methods() {
	BIND_GET_SET_RESOURCE(ComputeShaderTask, shader, ComputeShaderFile)
	BIND_GET_SET_OBJECT(ComputeShaderTask, rendering_device, RenderingDevice)
	BIND_METHOD(ComputeShaderTask, get_shader_parameter, "param")
	BIND_METHOD(ComputeShaderTask, set_shader_parameter, "param", "value")
	BIND_METHOD(ComputeShaderTask, clear_shader_parameters)
	ClassDB::bind_method(D_METHOD("dispatch", "kernel_name", "thread_groups", "context"), &ComputeShaderTask::dispatch, DEFVAL(nullptr));
	ClassDB::bind_method(D_METHOD("dispatch_at", "kernel_index", "thread_groups", "context"), &ComputeShaderTask::dispatch_at, DEFVAL(nullptr));
	ClassDB::bind_method(D_METHOD("dispatch_all", "thread_groups", "context"), &ComputeShaderTask::dispatch_all, DEFVAL(nullptr));
	ClassDB::bind_method(D_METHOD("dispatch_group", "group_name", "thread_groups", "context"), &ComputeShaderTask::dispatch_group, DEFVAL(nullptr));
	BIND_METHOD(ComputeShaderTask, get_rids, "param")
	BIND_METHOD(ComputeShaderTask, get_buffer_data, "param")
	BIND_METHOD(ComputeShaderTask, get_buffer_data_async, "param", "callback")
}

ComputeShaderTask::ComputeShaderTask() :
		_shader_object(nullptr) {}

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

RenderingDevice* ComputeShaderTask::get_rendering_device() const {
	return cast_to<RenderingDevice>(ObjectDB::get_instance(rendering_device_id));
}

void ComputeShaderTask::set_rendering_device(RenderingDevice* p_rendering_device) {
	ERR_FAIL_COND_MSG(p_rendering_device, "Overriding the rendering_device is not yet supported!");
	if (p_rendering_device != ObjectDB::get_instance(rendering_device_id)) {
		rendering_device_id = p_rendering_device ? p_rendering_device->get_instance_id() : ObjectID{};
		RenderingServer::get_singleton()->call_on_render_thread(callable_mp(this, &ComputeShaderTask::_reset));
	}
}

Variant ComputeShaderTask::get_shader_parameter(const StringName& param) const {
	const PackedStringArray parts = param.split("/");
	Variant current = _shader_parameters;
	int64_t i = 0;
	bool valid;
	for (; i < parts.size() - 1; ++i) {
		current = current.get_named(parts[i], valid);
		if (!valid || current.get_type() == Variant::NIL)
			return {};
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

TypedArray<RID> ComputeShaderTask::get_rids(const StringName& param) const {
	ERR_FAIL_NULL_V(_shader_object, {});
	return ComputeShaderCursor(_shader_object.get()).field(param).get_rids();
}

PackedByteArray ComputeShaderTask::get_buffer_data(const StringName& param) const {
	ERR_FAIL_NULL_V(_shader_object, {});
	return ComputeShaderCursor(_shader_object.get()).field(param).get_buffer_data();
}

Error ComputeShaderTask::get_buffer_data_async(const StringName& param, const Callable& callback) const {
	ERR_FAIL_NULL_V(_shader_object, {});
	return ComputeShaderCursor(_shader_object.get()).field(param).get_buffer_data_async(callback);
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
	if (p_name.begins_with(shader_param_prefix())) {
		const StringName param_name = p_name.substr(shader_param_prefix_length);
		set_shader_parameter(param_name, p_value);
		return true;
	}
	return false;
}

bool ComputeShaderTask::_get(const StringName& p_name, Variant& r_ret) const {
	if (p_name.begins_with(shader_param_prefix())) {
		const StringName param_name = p_name.substr(shader_param_prefix_length);
		const Dictionary params = get_shader_parameters();
		FieldShape field;
		if (_property_get_reflection(p_name, field)) {
			const Variant value = get_shader_parameter(param_name);
			if (value.get_type() == Variant::NIL && field.default_value.get_type() != Variant::NIL) {
				r_ret = field.default_value;
				return true;
			}
			if (field.property_info) {
				r_ret = UtilityFunctions::type_convert(value, field.property_info->type);
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
	_get_property_list(p_list, shader_param_prefix(), get_shader_parameters());
}

void ComputeShaderTask::_get_property_list(List<PropertyInfo>* p_list, const String& prefix, const Dictionary& properties) {
	ERR_FAIL_NULL(p_list);
	for (const StringName property_name : properties.keys()) {
		const FieldShape field = FieldShape::from_dict(properties[property_name]);
		if (field.property_info && _can_show_property_info(*field.property_info)) {
			PropertyInfo property_info = *field.property_info;
			property_info.name = prefix + property_info.name;
			p_list->push_back(property_info);
		} else if (const auto structured_shape = cast_to<StructTypeLayoutShape>(field.shape.ptr())) {
			_get_property_list(p_list, String("%s%s/") % TypedArray<String>{ prefix, property_name }, structured_shape->get_properties());
		}
	}
}

bool ComputeShaderTask::_can_show_property_info(const PropertyInfo& property_info) {
	return property_info.type != Variant::NIL && property_info.type != Variant::RID && (property_info.type != Variant::OBJECT || property_info.hint == PROPERTY_HINT_RESOURCE_TYPE);
}

bool ComputeShaderTask::_property_can_revert(const StringName& p_name) const {
	FieldShape field;
	return _property_get_reflection(p_name, field);
}

bool ComputeShaderTask::_property_get_revert(const StringName& p_name, Variant& r_property) const {
	FieldShape field;
	if (_property_get_reflection(p_name, field) && field.default_value.get_type() != Variant::NIL) {
		r_property = field.default_value;
		return true;
	}
	if (field.property_info) {
		r_property = UtilityFunctions::type_convert({}, field.property_info->type);
		return true;
	}
	return false;
}

bool ComputeShaderTask::_property_get_reflection(const StringName& p_name, FieldShape& r_reflection) const {
	if (p_name.begins_with(shader_param_prefix())) {
		const StringName param_name = p_name.substr(shader_param_prefix_length);
		const PackedStringArray parts = param_name.split("/");
		Variant current = get_shader_parameters();
		int64_t i = 0;
		bool valid;
		for (; i < parts.size() - 1; ++i) {
			const FieldShape field = FieldShape::from_dict(current.get_named(parts[i], valid));
			const auto structured_shape = cast_to<StructTypeLayoutShape>(field.shape.ptr());
			if (!valid || !structured_shape)
				return false;
			current = structured_shape->get_properties();
		}
		const Variant last = current.get_named(parts[i], valid);
		if (valid) {
			r_reflection = FieldShape::from_dict(last);
			return true;
		}
	}
	return false;
}

void ComputeShaderTask::_reset() {
	const int64_t num_kernels = get_kernels().size();
	_kernel_data.resize(num_kernels);
	for (int64_t i = 0; i < num_kernels; i++) {
		_kernel_data[i] = nullptr;
	}
	RenderingDevice* rd = _get_active_rendering_device();
	ERR_FAIL_NULL_MSG(rd, "ComputeShaderTask: Couldn't obtain rendering device for reset!");
	_sampler_cache = std::make_unique<SamplerCache>(rd);
	if (shader.is_valid() && shader->get_base_error().is_empty()) {
		_shader_object = std::make_unique<ComputeShaderObject>(rd, _sampler_cache.get(), shader->get_parameters());
	} else {
		_shader_object = nullptr;
	}
}

void ComputeShaderTask::_shader_changed() {
	_reset();
	notify_property_list_changed();
}

RenderingDevice* ComputeShaderTask::_get_active_rendering_device() const {
	if (RenderingDevice* rendering_device = get_rendering_device()) {
		return rendering_device;
	}
	const RenderingServer* rendering_server = RenderingServer::get_singleton();
	return rendering_server ? rendering_server->get_rendering_device() : nullptr;
}

ComputeShaderTask::KernelData* ComputeShaderTask::_get_or_create_kernel(const int64_t kernel_index) {
	ERR_FAIL_INDEX_V(kernel_index, _kernel_data.size(), nullptr);
	std::unique_ptr<KernelData>& kernel_data = _kernel_data[kernel_index];
	if (kernel_data) {
		return kernel_data.get();
	}
	RenderingDevice* rd = _get_active_rendering_device();
	ERR_FAIL_NULL_V(rd, nullptr);
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	ERR_FAIL_INDEX_V(kernel_index, kernels.size(), nullptr);
	const Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
	const RID shader_rid = rd->shader_create_from_spirv(kernel->get_spirv(), shader->get_name().get_file());
	kernel_data = std::make_unique<KernelData>(KernelData{
		{},
		UniqueRID(rd, shader_rid),
		UniqueRID(rd, rd->compute_pipeline_create(shader_rid)),
		std::make_unique<ComputeShaderObject>(rd, _sampler_cache.get(), kernel->get_parameters()),
	});
	return kernel_data.get();
}

void ComputeShaderTask::_dispatch(const int64_t kernel_index, const Vector3i thread_groups, const Object* context) {
	ERR_FAIL_NULL(shader);
	ERR_FAIL_NULL(_shader_object);
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	ERR_FAIL_INDEX_MSG(kernel_index, kernels.size(), String("Attempted to dispatch invalid kernel index %s (max %s)!") % PackedStringArray({ String::num_int64(kernel_index), String::num_int64(kernels.size() - 1) }));

	const Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
	ERR_FAIL_NULL_MSG(kernel, String("Attempted to dispatch invalid kernel index %s (found: nil)!") % String::num_int64(kernel_index));
	ERR_FAIL_COND_MSG(!kernel->get_compile_error().is_empty(), "Can't dispatch kernel with compile error!");

	RenderingDevice* rendering_device = _get_active_rendering_device();
	ERR_FAIL_NULL_MSG(rendering_device, "ComputeShaderTask: Couldn't obtain rendering device for dispatch!");

	const KernelData* kernel_data = _get_or_create_kernel(kernel_index);
	ERR_FAIL_NULL_MSG(kernel_data, "ComputeShaderTask: Couldn't obtain kernel data!");

	ComputeShaderCursor(_shader_object.get(), context).write(_shader_parameters);
	ComputeShaderCursor(kernel_data->shader_object.get(), context).write(kernel_data->parameters);
	_shader_object->flush_buffers();
	kernel_data->shader_object->flush_buffers();
	const int64_t compute_list = rendering_device->compute_list_begin();
	rendering_device->compute_list_bind_compute_pipeline(compute_list, kernel_data->pipeline_rid);

	const ComputeShaderObject::DescriptorSets descriptor_sets = _shader_object->get_descriptor_sets();
	for (const auto& [space_index, uniforms] : descriptor_sets) {
		if (kernel->get_used_binding_sets().get(space_index, false)) {
			// TODO: UniformSetCacheRD only works with the default rendering device
			const RID uniform_set = UniformSetCacheRD::get_cache(kernel_data->shader_rid, space_index, uniforms);
			rendering_device->compute_list_bind_uniform_set(compute_list, uniform_set, space_index);
		}
	}
	const PackedByteArray& push_constants = _shader_object->get_push_constants();
	if (push_constants.size() > 0) {
		rendering_device->compute_list_set_push_constant(compute_list, push_constants, push_constants.size());
	}

	rendering_device->compute_list_dispatch(compute_list, thread_groups.x, thread_groups.y, thread_groups.z);
	rendering_device->compute_list_end();
}
