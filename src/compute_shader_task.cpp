#include "godot_cpp/classes/editor_file_system.hpp"
#include "godot_cpp/classes/editor_interface.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/time.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/classes/uniform_set_cache_rd.hpp"
#include "godot_cpp/classes/window.hpp"

#include "compute_shader_task.h"
#include "compute_shader_shape.h"

void ComputeShaderTask::_bind_methods() {
	BIND_GET_SET_RESOURCE(ComputeShaderTask, shader, ComputeShaderFile)
	BIND_METHOD(ComputeShaderTask, get_shader_parameter, "param")
	BIND_METHOD(ComputeShaderTask, set_shader_parameter, "param", "value")
	BIND_METHOD(ComputeShaderTask, clear_shader_parameters)
	BIND_METHOD(ComputeShaderTask, dispatch, "kernel_name", "thread_groups")
	BIND_METHOD(ComputeShaderTask, dispatch_at, "kernel_index", "thread_groups")
	BIND_METHOD(ComputeShaderTask, dispatch_all, "thread_groups")
}

ComputeShaderTask::ComputeShaderTask() {
	_linear_sampler_cache.resize(RenderingDevice::SAMPLER_REPEAT_MODE_MAX);
	_nearest_sampler_cache.resize(RenderingDevice::SAMPLER_REPEAT_MODE_MAX);
}

ComputeShaderTask::~ComputeShaderTask() {
	_reset();
	const RenderingServer* rendering_server = RenderingServer::get_singleton();
	if (RenderingDevice* rd = rendering_server ? rendering_server->get_rendering_device() : nullptr) {
		for (int64_t i = 0; i < RenderingDevice::SAMPLER_REPEAT_MODE_MAX; ++i) {
			if (RID rid = _nearest_sampler_cache[i]; rid.is_valid()) {
				rd->free_rid(rid);
			}
			if (RID rid = _linear_sampler_cache[i]; rid.is_valid()) {
				rd->free_rid(rid);
			}
		}
	}
}

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

void ComputeShaderTask::dispatch_all(const Vector3i thread_groups) {
	ERR_FAIL_NULL(shader);
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	for (int64_t i = 0; i < kernels.size(); i++) {
		_dispatch(i, thread_groups);
	}
}

void ComputeShaderTask::dispatch(const StringName& kernel_name, const Vector3i thread_groups) {
	ERR_FAIL_NULL(shader);
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	for (int64_t i = 0; i < kernels.size(); i++) {
		Ref<ComputeShaderKernel> kernel = kernels[i];
		if (kernel.is_valid() && kernel->get_kernel_name() == kernel_name) {
			_dispatch(i, thread_groups);
		}
	}
}

void ComputeShaderTask::dispatch_at(const int64_t kernel_index, const Vector3i thread_groups) {
	_dispatch(kernel_index, thread_groups);
}

Dictionary ComputeShaderTask::get_shader_parameters() const {
	if (shader.is_null()) {
		return {};
	}
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	Dictionary shader_params;
	for (const Ref<ComputeShaderKernel> kernel : kernels) {
		if (kernel.is_valid()) {
			Dictionary params = kernel->get_parameters();
			for (const Variant& param_name : params.keys()) {
				shader_params.set(param_name, params[param_name]);
			}
		}
	}
	return shader_params;
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
		const Dictionary reflection = params.get(param_name, Dictionary());
		if (reflection.has("property_info")) {
			const PropertyInfo property_info = PropertyInfo::from_dict(reflection["property_info"]);
			r_ret = UtilityFunctions::type_convert(get_shader_parameter(param_name), property_info.type);
			return true;
		}
		r_ret = get_shader_parameter(param_name);
		return true;
	}
	return false;
}

void ComputeShaderTask::_get_property_list(List<PropertyInfo>* p_list) const {
	Dictionary params = get_shader_parameters();
	for (const StringName param_name : params.keys()) {
		const Dictionary param_info = params.get(param_name, Dictionary());
		PropertyInfo property_info = PropertyInfo::from_dict(param_info.get("property_info", Dictionary()));
		property_info.name = "shader_parameter/" + property_info.name;
		if (property_info.type != Variant::NIL && (property_info.type != Variant::OBJECT || property_info.hint == PROPERTY_HINT_RESOURCE_TYPE) && property_info.type != Variant::RID) {
			p_list->push_back(property_info);
		} else {
			_get_property_list(p_list, "shader_parameter/" + param_name + "/", param_info["shape"]);
		}
	}
}

void ComputeShaderTask::_get_property_list(List<PropertyInfo>* p_list, const String& prefix, const Ref<ShaderTypeLayoutShape>& shape) const {
	const StructTypeLayoutShape* structured_shape = cast_to<StructTypeLayoutShape>(shape.ptr());
	if (!structured_shape) return;

	const Dictionary properties = structured_shape->get_properties();
	for (const StringName property_name : properties.keys()) {
		const Dictionary property = properties.get(property_name, Dictionary());
		PropertyInfo property_info = PropertyInfo::from_dict(property.get("property_info", Dictionary()));
		property_info.name = prefix + property_info.name;
		if (property_info.type != Variant::NIL && (property_info.type != Variant::OBJECT || property_info.hint == PROPERTY_HINT_RESOURCE_TYPE) && property_info.type != Variant::RID) {
			p_list->push_back(property_info);
		} else {
			_get_property_list(p_list, prefix + property_name + "/", property["shape"]);
		}
	}
}

bool ComputeShaderTask::_property_can_revert(const StringName& p_name) const {
	if (p_name.begins_with("shader_parameter/")) {
		const PackedStringArray parts = p_name.substr(17).split("/");
		Variant current = get_shader_parameters();
		int64_t i = 0;
		bool valid;
		for (; i < parts.size() - 1; ++i) {
			const Dictionary property = current.get_named(parts[i], valid);
			const Ref<ShaderTypeLayoutShape> shape = property["shape"];
			const auto structured_shape = cast_to<StructTypeLayoutShape>(shape.ptr());
			if (!valid || !structured_shape) return false;
			current = structured_shape->get_properties();
		}
		const Dictionary reflection = current.get_named(parts[i], valid);
		if (valid && reflection.has("property_info")) {
			const PropertyInfo property_info = PropertyInfo::from_dict(reflection["property_info"]);
			return UtilityFunctions::type_convert(get(p_name), property_info.type) != UtilityFunctions::type_convert(nullptr, property_info.type);
		}
	}
	return false;
}

bool ComputeShaderTask::_property_get_revert(const StringName& p_name, Variant& r_property) const {
	if (p_name.begins_with("shader_parameter/")) {
		r_property = nullptr;
		return true;
	}
	return false;
}

void ComputeShaderTask::_reset() {
	const RenderingServer* rendering_server = RenderingServer::get_singleton();
	if (RenderingDevice* rd = rendering_server ? rendering_server->get_rendering_device() : nullptr) {
		for (const Variant& key : _kernel_pipelines.keys()) {
			const RID rid = _kernel_pipelines[key];
			if (rid.is_valid()) {
				rd->free_rid(_kernel_pipelines[key]);
			}
		}
		for (const Variant& key : _kernel_shaders.keys()) {
			const RID rid = _kernel_shaders[key];
			if (rid.is_valid()) {
				rd->free_rid(_kernel_shaders[key]);
			}
		}
	}
	_push_constant.clear();
	_buffers.clear();
	_kernel_pipelines.clear();
	_kernel_shaders.clear();
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
		_kernel_shaders.set(kernel_index, rd->shader_create_from_spirv(kernel->get_spirv()));
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

RID ComputeShaderTask::_get_sampler(const RenderingDevice::SamplerFilter filter, const RenderingDevice::SamplerRepeatMode repeat_mode) const {
	ERR_FAIL_INDEX_V(repeat_mode, RenderingDevice::SAMPLER_REPEAT_MODE_MAX, RID{});
	TypedArray<RID> sampler_cache = filter == RenderingDevice::SamplerFilter::SAMPLER_FILTER_NEAREST ? _nearest_sampler_cache : _linear_sampler_cache;
	if (RID cached_value = sampler_cache[repeat_mode]; cached_value.is_valid()) {
		return cached_value;
	}

	const RenderingServer* rendering_server = RenderingServer::get_singleton();
	ERR_FAIL_NULL_V(rendering_server, RID{});
	RenderingDevice* rd = rendering_server->get_rendering_device();
	ERR_FAIL_NULL_V(rd, RID{});

	const Ref sampler_state = memnew(RDSamplerState);
	sampler_state->set_min_filter(filter);
	sampler_state->set_mag_filter(filter);
	sampler_state->set_mip_filter(filter);
	sampler_state->set_repeat_u(repeat_mode);
	sampler_state->set_repeat_v(repeat_mode);
	sampler_state->set_repeat_w(repeat_mode);
	const RID sampler_rid = rd->sampler_create(sampler_state);
	sampler_cache[repeat_mode] = sampler_rid;
	return sampler_rid;
}

Variant ComputeShaderTask::_get_parameter_value(const StringName& param_name, const RenderingDevice::UniformType uniform_type, const Dictionary& attributes) const {
	Variant value = _shader_parameters.get(param_name, Variant());
	if (value.get_type() == Variant::NIL) {
		value = _get_default_uniform(uniform_type, attributes);
	}
	if (attributes.has("gd_Color")) {
		if (value.get_type() == Variant::COLOR) {
			const Color color = value;
			return color.srgb_to_linear();
		}
		if (value.get_type() == Variant::PACKED_COLOR_ARRAY) {
			PackedColorArray colors = value.duplicate();
			for (Color& color: colors) {
				color = color.srgb_to_linear();
			}
			return colors;
		}
	}
	return value;
}

void ComputeShaderTask::_update_buffers(const int64_t kernel_index) {
	ERR_FAIL_NULL(shader);
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	const Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
	Dictionary parameters = kernel->get_parameters();
	Array parameter_keys = parameters.keys();
	for (const Variant& parameter_key : parameter_keys) {
		const StringName& key = parameter_key;
		const Dictionary param = parameters.get(key, Dictionary());
		const auto uniform_type = static_cast<RenderingDevice::UniformType>(static_cast<int32_t>(param.get("uniform_type", -1)));
		const StringName param_name = param.get("name", StringName{});
		if (param_name.is_empty()) continue;
		const Dictionary attributes = param["user_attributes"];
		if (!param.has("uniform_type")) {
			Variant value = _get_parameter_value(param_name, uniform_type, attributes);
			Ref<ShaderTypeLayoutShape> shape = param["shape"];
			if (shape.is_valid()) {
				const int64_t size = shape->get_size();
				if (size > 0) {
					const int64_t offset = param.get("offset", 0);
					const int64_t buffer_size = RDBuffer::aligned_size(offset + size, 16);
					if (_push_constant.size() < buffer_size) {
						_push_constant.resize(buffer_size);
					}
					RDBuffer::write_shape(_push_constant, offset, shape.ptr(), value, true);
				}
			}
		} else if (uniform_type == RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER || uniform_type == RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER) {
			Variant value = _shader_parameters.get(param_name, Variant());
			RID value_rid = value;
			const int32_t binding_space = param.get("binding_space", 0);
			const int32_t binding_index = param.get("binding_index", 0);
			if (value_rid.is_valid()) {
				_set_buffer(binding_index, binding_space, value_rid);
			} else {
				value = _get_parameter_value(param_name, uniform_type, attributes);
				Ref<ShaderTypeLayoutShape> shape = param["shape"];
				const Ref<RDBuffer> buffer = _get_buffer(binding_index, binding_space);
				const int64_t offset = param.get("offset", 0);
				buffer->write_shape(offset, shape.ptr(), value);
			}
		}
	}

	for (const Ref<RDBuffer> buffer : _buffers.values()) {
		if (buffer.is_valid()) {
			buffer->flush();
		}
	}
}

void ComputeShaderTask::_bind_uniform_sets(const int64_t kernel_index, const int64_t compute_list, RenderingDevice* rd) {
	ERR_FAIL_NULL(shader);
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	Dictionary uniform_sets{};

	const Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
	Dictionary parameters = kernel->get_parameters();
	Array parameter_keys = parameters.keys();
	for (const StringName key : parameter_keys) {
		const Dictionary param = parameters[key];
		const int64_t binding_space = param.get("binding_space", 0);
		const int32_t binding_index = param.get("binding_index", 0);
		const int64_t param_type = param.get("uniform_type", -1);
		if (param_type == RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER || param_type == RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER)
			continue;

		const StringName param_name = param.get("name", StringName{});
		if (!param_name.is_empty() && param_type >= 0 && param_type < RenderingDevice::UniformType::UNIFORM_TYPE_MAX) {
			const auto uniform_type = static_cast<RenderingDevice::UniformType>(param_type);
			const Dictionary attributes = param["user_attributes"];
			Variant value = _get_parameter_value(param_name, uniform_type, attributes);
			if (const Object* object = value) {
				if (object->is_class("Texture")) {
					value = RenderingServer::get_singleton()->texture_get_rd_texture(value);
				}
			}
			if (value.get_type() != Variant::NIL) {
				Ref uniform = memnew(RDUniform);
				uniform->set_binding(binding_index);
				uniform->set_uniform_type(uniform_type);
				if (uniform_type == RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE && value.get_type() != Variant::ARRAY) {
					Variant sampler = _get_default_uniform(RenderingDevice::UNIFORM_TYPE_SAMPLER, attributes);
					if (sampler.get_type() == Variant::NIL) {
						sampler = _get_sampler(RenderingDevice::SAMPLER_FILTER_LINEAR, RenderingDevice::SamplerRepeatMode::SAMPLER_REPEAT_MODE_REPEAT);
					}
					uniform->add_id(sampler);
					uniform->add_id(value);
				} else if (value.get_type() == Variant::ARRAY) {
					Array array = value;
					for (const Variant& id: array) {
						uniform->add_id(id);
					}
				} else {
					uniform->add_id(value);
				}
				TypedArray<RDUniform> uniforms = uniform_sets.get_or_add(binding_space, TypedArray<RDUniform>{});
				uniforms.push_back(uniform);
			}
		}
	}

	for (const Vector2i key : _buffers.keys()) {
		const Ref<RDBuffer> buffer = _buffers[key];
		if (buffer.is_valid()) {
			const Ref uniform = memnew(RDUniform);
			uniform->set_binding(key.x);
			uniform->set_uniform_type(buffer->get_uniform_type());
			uniform->add_id(buffer->get_rid());
			TypedArray<RDUniform> uniforms = uniform_sets.get_or_add(key.y, TypedArray<RDUniform>{});
			uniforms.push_back(uniform);
		}
	}

	const RID shader_rid = _get_shader_rid(kernel_index, rd);
	for (const uint32_t key : uniform_sets.keys()) {
		const TypedArray<RDUniform> value = uniform_sets.get(key, TypedArray<RDUniform>{});
		RID uniform_set = UniformSetCacheRD::get_cache(shader_rid, key, value);
		rd->compute_list_bind_uniform_set(compute_list, uniform_set, key);
	}
}

Variant ComputeShaderTask::_get_default_uniform(const RenderingDevice::UniformType type, Dictionary user_attributes) const {
	if (user_attributes.has("gd_GlobalParam")) {
		const Dictionary attribute = user_attributes["gd_GlobalParam"];
		const String param_name = attribute["name"];
		return RenderingServer::get_singleton()->global_shader_parameter_get(param_name);
	}
	switch (type) {
		case RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER:
		case RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER:
			if (user_attributes.has("gd_Time")) {
				return Time::get_singleton()->get_ticks_msec() * .001f;
			}
			if (user_attributes.has("gd_FrameId")) {
				return Engine::get_singleton()->get_frames_drawn();
			}
			if (user_attributes.has("gd_MousePosition")) {
				if (const SceneTree* scene_tree = cast_to<SceneTree>(Engine::get_singleton()->get_main_loop())) {
					if (const Window* window = scene_tree->get_root()) {
						return Vector2i(window->get_mouse_position());
					}
				}
			}
			break;
		case RenderingDevice::UNIFORM_TYPE_SAMPLER:
			if (user_attributes.has("gd_LinearSampler")) {
				const Dictionary sampler_attribute = user_attributes["gd_LinearSampler"];
				int64_t repeat_mode_int = sampler_attribute.get("repeat_mode", RenderingDevice::SAMPLER_REPEAT_MODE_REPEAT);
				return _get_sampler(RenderingDevice::SAMPLER_FILTER_LINEAR, static_cast<RenderingDevice::SamplerRepeatMode>(repeat_mode_int));
			}
			if (user_attributes.has("gd_NearestSampler")) {
				const Dictionary sampler_attribute = user_attributes["gd_NearestSampler"];
				int64_t repeat_mode_int = sampler_attribute.get("repeat_mode", RenderingDevice::SAMPLER_REPEAT_MODE_REPEAT);
				return _get_sampler(RenderingDevice::SAMPLER_FILTER_NEAREST, static_cast<RenderingDevice::SamplerRepeatMode>(repeat_mode_int));
			}
			return _get_sampler(RenderingDevice::SAMPLER_FILTER_LINEAR, RenderingDevice::SAMPLER_REPEAT_MODE_REPEAT);
		case RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE:
		case RenderingDevice::UNIFORM_TYPE_TEXTURE: {
			const RID default_texture = user_attributes.has("gd_DefaultWhite")
				? RenderingServer::get_singleton()->get_white_texture()
				: RenderingServer::get_singleton()->get_test_texture();
			return RenderingServer::get_singleton()->texture_get_rd_texture(default_texture);
		}
		default:
			break;
	}
	return nullptr;
}

Ref<RDBuffer> ComputeShaderTask::_get_buffer(const int32_t binding, const int32_t set) {
	ERR_FAIL_NULL_V(shader, {});
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	const Vector2i key(binding, set);
	if (!_buffers.has(key)) {
		const Ref buffer = memnew(RDBuffer);
		_buffers.set(key, buffer);
		if (kernels.size() > 0) {
			const Ref<ComputeShaderKernel> kernel = kernels[0];
			if (kernel.is_valid()) {
				const TypedArray<Dictionary> buffers = kernel->get_buffers();
				const int64_t buffer_count = buffers.size();
				for (int64_t i = 0; i < buffer_count; i++) {
					Dictionary buffer_info = buffers[i];
					const int64_t info_index = buffer_info["binding_index"];
					const int64_t info_set = buffer_info["binding_space"];
					if (info_index == binding && info_set == set) {
						// TODO: This whole method should be refactored to make sense, but this works for now
						const int64_t buffer_size = buffer_info["size"];
						buffer->set_size(buffer_size);
						buffer->set_is_fixed_size(true);
					}
				}
			}
		}
		return buffer;
	}
	return _buffers[key];
}

void ComputeShaderTask::_set_buffer(const int32_t binding, const int32_t set, const RID& buffer_rid) {
	const Vector2i key(binding, set);
	_buffers.set(key, RDBuffer::ref(buffer_rid));
}

void ComputeShaderTask::_dispatch(const int64_t kernel_index, const Vector3i thread_groups) {
	ERR_FAIL_NULL(shader);
	const TypedArray<ComputeShaderKernel>& kernels = shader->get_kernels();
	ERR_FAIL_INDEX_MSG(kernel_index, kernels.size(), String("Attempted to dispatch invalid kernel index %s (max %s)!") % PackedStringArray({ String::num_int64(kernel_index), String::num_int64(kernels.size() - 1) }));
	ERR_FAIL_NULL_MSG(static_cast<Ref<RefCounted>>(kernels[kernel_index]), String("Attempted to dispatch invalid kernel index %s (found: nil)!") % String::num_int64(kernel_index));
	RenderingServer* rendering_server = RenderingServer::get_singleton();
	ERR_FAIL_NULL_MSG(rendering_server, "ComputeShaderTask: Couldn't obtain RenderingServer for dispatch!");
	ERR_FAIL_COND(!rendering_server->is_on_render_thread());
	RenderingDevice* rendering_device = rendering_server->get_rendering_device();
	ERR_FAIL_NULL_MSG(rendering_device, "ComputeShaderTask: Couldn't obtain rendering device for dispatch!");

	_update_buffers(kernel_index);
	const int64_t compute_list = rendering_device->compute_list_begin();
	const RID pipeline = _get_shader_pipeline_rid(kernel_index, rendering_device);
	rendering_device->compute_list_bind_compute_pipeline(compute_list, pipeline);
	_bind_uniform_sets(kernel_index, compute_list, rendering_device);
	if (!_push_constant.is_empty()) {
		rendering_device->compute_list_set_push_constant(compute_list, _push_constant, _push_constant.size());
	}
	rendering_device->compute_list_dispatch(compute_list, thread_groups.x, thread_groups.y, thread_groups.z);
	rendering_device->compute_list_end();
}
