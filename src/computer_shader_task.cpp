#include "computer_shader_task.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/time.hpp"
#include "godot_cpp/classes/uniform_set_cache_rd.hpp"

#define BUFFER_COPY(T)          \
	const T source_data = data; \
	memcpy(buffer.ptrw() + offset, source_data.ptr(), Math::min(size, source_data.size()));

void ComputeShaderTask::_bind_methods() {
	BIND_GET_SET_RESOURCE_ARRAY(ComputeShaderTask, kernels, ComputeShaderKernel)
	BIND_METHOD(ComputeShaderTask, get_shader_parameter, "param")
	BIND_METHOD(ComputeShaderTask, set_shader_parameter, "param", "value")
	BIND_METHOD(ComputeShaderTask, dispatch, "kernel_name", "thread_groups")
	BIND_METHOD(ComputeShaderTask, dispatch_at, "kernel_index", "thread_groups")
	BIND_METHOD(ComputeShaderTask, dispatch_all, "thread_groups")
}

void RDUniformBuffer::_bind_methods() {
}

RDUniformBuffer::~RDUniformBuffer() {
	if (rid.is_valid()) {
		RenderingServer::get_singleton()->free_rid(rid);
	}
}

void RDUniformBuffer::write(const int64_t offset, const int64_t size, const Variant& data) {
	if (buffer.size() == 0) {
		UtilityFunctions::push_error("Writing uniform buffer before initialize!");
		return;
	}
	switch (data.get_type()) {
		case Variant::BOOL:
			buffer.encode_s32(offset, data ? 1 : 0);
			break;
		case Variant::INT:
			buffer.encode_s32(offset, data);
			break;
		case Variant::FLOAT:
			buffer.encode_float(offset, data);
			break;
		case Variant::VECTOR2: {
			const Vector2 vector = data;
			buffer.encode_float(offset, vector.x);
			buffer.encode_float(offset + 4, vector.y);
			break;
		}
		case Variant::VECTOR3: {
			const Vector3 vector = data;
			buffer.encode_float(offset, vector.x);
			buffer.encode_float(offset + 4, vector.y);
			buffer.encode_float(offset + 8, vector.z);
			break;
		}
		case Variant::VECTOR4: {
			const Vector4 vector = data;
			buffer.encode_float(offset, vector.x);
			buffer.encode_float(offset + 4, vector.y);
			buffer.encode_float(offset + 8, vector.z);
			buffer.encode_float(offset + 12, vector.w);
			break;
		}
		case Variant::COLOR: {
			const Color color = data;
			buffer.encode_float(offset, color.r);
			buffer.encode_float(offset + 4, color.g);
			buffer.encode_float(offset + 8, color.b);
			if (size >= 16) {
				buffer.encode_float(offset + 12, color.a);
			}
			break;
		}
		case Variant::VECTOR2I: {
			const Vector2i vector = data;
			buffer.encode_s32(offset, vector.x);
			buffer.encode_s32(offset + 4, vector.y);
			break;
		}
		case Variant::VECTOR3I: {
			const Vector3i vector = data;
			buffer.encode_s32(offset, vector.x);
			buffer.encode_s32(offset + 4, vector.y);
			buffer.encode_s32(offset + 8, vector.z);
			break;
		}
		case Variant::VECTOR4I: {
			const Vector4i vector = data;
			buffer.encode_s32(offset, vector.x);
			buffer.encode_s32(offset + 4, vector.y);
			buffer.encode_s32(offset + 8, vector.z);
			buffer.encode_s32(offset + 12, vector.w);
			break;
		}
		case Variant::PACKED_BYTE_ARRAY: {
			BUFFER_COPY(PackedByteArray)
			break;
		}
		case Variant::PACKED_INT32_ARRAY: {
			BUFFER_COPY(PackedInt32Array)
			break;
		}
		case Variant::PACKED_INT64_ARRAY: {
			BUFFER_COPY(PackedInt64Array)
			break;
		}
		case Variant::PACKED_FLOAT32_ARRAY: {
			BUFFER_COPY(PackedFloat32Array)
			break;
		}
		case Variant::PACKED_FLOAT64_ARRAY: {
			BUFFER_COPY(PackedFloat64Array)
			break;
		}
		case Variant::PACKED_VECTOR2_ARRAY: {
			BUFFER_COPY(PackedVector2Array)
			break;
		}
		case Variant::PACKED_VECTOR3_ARRAY: {
			BUFFER_COPY(PackedVector3Array)
			break;
		}
		case Variant::PACKED_VECTOR4_ARRAY: {
			BUFFER_COPY(PackedVector4Array)
			break;
		}
		case Variant::PACKED_COLOR_ARRAY: {
			BUFFER_COPY(PackedColorArray)
			break;
		}
		case Variant::PACKED_STRING_ARRAY: {
			const PackedStringArray array = data;
			const PackedByteArray byte_array = array.to_byte_array();
			memcpy(buffer.ptrw() + offset, byte_array.ptr(), Math::min(size, byte_array.size()));
			break;
		}
		default:
			break;
	}
	if (RenderingDevice* rd = RenderingServer::get_singleton()->get_rendering_device()) {
		if (!rid.is_valid()) {
			rid = rd->uniform_buffer_create(buffer.size(), buffer);
		} else {
			// TODO: Avoid updating the full buffer every frame
			rd->buffer_update(rid, 0, buffer.size(), buffer);
		}
	}
}

void RDUniformBuffer::set_size(const int64_t size) {
	buffer.resize(size);
}

Ref<RDUniformBuffer> RDUniformBuffer::ref(const RID& buffer_rid) {
	Ref result = memnew(RDUniformBuffer);
	result->set_rid(buffer_rid);
	return result;
}

GET_SET_PROPERTY_IMPL(RDUniformBuffer, RID, rid)
GET_SET_PROPERTY_IMPL(RDUniformBuffer, PackedByteArray, buffer)

ComputeShaderTask::ComputeShaderTask() {
	_linear_sampler_cache.resize(RenderingDevice::SAMPLER_REPEAT_MODE_MAX);
	_nearest_sampler_cache.resize(RenderingDevice::SAMPLER_REPEAT_MODE_MAX);
}

ComputeShaderTask::~ComputeShaderTask() {
	if (RenderingDevice* rd = RenderingServer::get_singleton()->get_rendering_device()) {
		const Array pipeline_keys = _kernel_pipelines.keys();
		for (size_t i = 0; i < pipeline_keys.size(); ++i) {
			if (RID key = pipeline_keys[i]; key.is_valid()) {
				rd->free_rid(_kernel_pipelines[key]);
			}
		}
		const Array shader_keys = _kernel_shaders.keys();
		for (size_t i = 0; i < shader_keys.size(); ++i) {
			if (RID key = shader_keys[i]; key.is_valid()) {
				rd->free_rid(_kernel_shaders[key]);
			}
		}
		for (size_t i = 0; i < RenderingDevice::SAMPLER_REPEAT_MODE_MAX; ++i) {
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
	return kernels;
}

void ComputeShaderTask::set_kernels(TypedArray<ComputeShaderKernel> p_kernels) {
	kernels = p_kernels;
}

Variant ComputeShaderTask::get_shader_parameter(const StringName& param) const {
	return _shader_parameters.get(param, nullptr);
}

void ComputeShaderTask::set_shader_parameter(const StringName& param, const Variant& value) {
	_shader_parameters.set(param, value);
}

void ComputeShaderTask::dispatch_all(const Vector3i thread_groups) {
	for (int64_t i = 0; i < kernels.size(); i++) {
		_dispatch(i, thread_groups);
	}
}

void ComputeShaderTask::dispatch(const StringName& kernel_name, const Vector3i thread_groups) {
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

RID ComputeShaderTask::_get_shader_rid(const int64_t kernel_index, RenderingDevice* rd) {
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
	if (RenderingDevice* rd = RenderingServer::get_singleton()->get_rendering_device()) {
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
	return {};
}

void ComputeShaderTask::_update_buffers(const int64_t kernel_index) {
	const Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
	Dictionary parameters = kernel->get_parameters();
	Array parameter_keys = parameters.keys();
	for (const Variant& parameter_key : parameter_keys) {
		const StringName& key = parameter_key;
		const Dictionary param = parameters[key];
		const int64_t param_type = param.get("type", -1);
		const StringName param_name = param.get("name", StringName{});
		const int64_t binding_space = param.get("binding_space", 0);
		const int64_t binding_index = param.get("binding_index", 0);
		if (!param_name.is_empty() && param_type == RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER) {
			Variant value = _shader_parameters[param_name];
			RID value_rid = value;
			if (value_rid.is_valid()) {
				_set_uniform_buffer(binding_index, binding_space, value_rid);
			} else {
				if (value == Variant(nullptr)) {
					value = _get_default_uniform(RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER, param["user_attributes"]);
				}

				const Ref<RDUniformBuffer> buffer = _get_uniform_buffer(binding_index, binding_space);
				const int64_t offset = param.get("offset", 0);
				const int64_t size = param.get("size", 0);
				if (size > 0) {
					buffer->write(offset, size, value);
				}
			}
		}
	}
}

void ComputeShaderTask::_bind_uniform_sets(const int64_t kernel_index, const int64_t compute_list, RenderingDevice* rd) {
	Dictionary uniform_sets{};

	const Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
	Dictionary parameters = kernel->get_parameters();
	Array parameter_keys = parameters.keys();
	for (uint32_t param_index = 0; param_index < parameter_keys.size(); param_index++) {
		const StringName& key = parameter_keys[param_index];
		const Dictionary param = parameters[key];
		const int64_t binding_space = param.get("binding_space", 0);
		const int64_t binding_index = param.get("binding_index", 0);
		const int64_t param_type = param.get("type", -1);
		if (param_type == RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER)
			continue;

		const StringName param_name = param.get("name", StringName{});
		if (!param_name.is_empty() && param_type >= 0 && param_type < RenderingDevice::UniformType::UNIFORM_TYPE_MAX) {
			const auto uniform_type = static_cast<RenderingDevice::UniformType>(param_type);
			Variant value = _shader_parameters[param_name];
			if (value.get_type() == Variant::NIL) {
				value = _get_default_uniform(uniform_type, param["user_attributes"]);
			}
			if (value.get_type() != Variant::NIL) {
				Ref uniform = memnew(RDUniform);
				uniform->set_binding(binding_index);
				uniform->set_uniform_type(uniform_type);
				if (uniform_type == RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE && value.get_type() != Variant::ARRAY) {
					Variant sampler = _get_default_uniform(RenderingDevice::UNIFORM_TYPE_SAMPLER, param["user_attributes"]);
					if (sampler.get_type() == Variant::NIL) {
						sampler = _get_sampler(RenderingDevice::SAMPLER_FILTER_LINEAR, RenderingDevice::SamplerRepeatMode::SAMPLER_REPEAT_MODE_REPEAT);
					}
					uniform->add_id(sampler);
					uniform->add_id(value);
				} else if (value.get_type() == Variant::ARRAY) {
					Array array = value;
					for (size_t i = 0; i < array.size(); ++i) {
						uniform->add_id(array[i]);
					}
				} else {
					uniform->add_id(value);
				}
				TypedArray<RDUniform> uniforms = uniform_sets.get_or_add(binding_space, TypedArray<RDUniform>{});
				uniforms.push_back(uniform);
			}
		}
	}

	const Array uniform_buffer_keys = _uniform_buffers.keys();
	for (uint32_t i = 0; i < uniform_buffer_keys.size(); i++) {
		Vector2i key = uniform_buffer_keys[i];
		const Ref<RDUniformBuffer> buffer = _uniform_buffers[key];
		if (buffer.is_valid()) {
			const Ref uniform = memnew(RDUniform);
			uniform->set_binding(key.x);
			uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);
			uniform->add_id(buffer->get_rid());
			TypedArray<RDUniform> uniforms = uniform_sets.get_or_add(key.y, TypedArray<RDUniform>{});
			uniforms.push_back(uniform);
		}
	}

	const RID shader_rid = _get_shader_rid(kernel_index, rd);
	const Array uniform_set_keys = uniform_sets.keys();
	for (uint32_t i = 0; i < uniform_set_keys.size(); i++) {
		const uint32_t key = uniform_set_keys[i];
		const TypedArray<RDUniform> value = uniform_sets.get(key, TypedArray<RDUniform>{});
		RID uniform_set = UniformSetCacheRD::get_cache(shader_rid, key, value);
		rd->compute_list_bind_uniform_set(compute_list, uniform_set, key);
	}
}

Variant ComputeShaderTask::_get_default_uniform(const RenderingDevice::UniformType type, Dictionary user_attributes) const {
	if (user_attributes.has("gd_GlobalParam")) {
		const Dictionary attribute = user_attributes["gd_GlobalParam"];
		const String param_name = attribute["name"];
		// TODO: This doesn't work for textures?
		return RenderingServer::get_singleton()->global_shader_parameter_get(param_name);
	}
	switch (type) {
		case RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER:
			if (user_attributes.has("gd_Time")) {
				return Time::get_singleton()->get_ticks_msec() * .001f;
			}
			if (user_attributes.has("gd_FrameId")) {
				return Engine::get_singleton()->get_frames_drawn();
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
			break;
		case RenderingDevice::UNIFORM_TYPE_TEXTURE:
			break;
		default:
			break;
	}
	return nullptr;
}

Ref<RDUniformBuffer> ComputeShaderTask::_get_uniform_buffer(const int64_t binding, const int64_t set) {
	const Vector2i key(binding, set);
	if (!_uniform_buffers.has(key)) {
		const Ref buffer = memnew(RDUniformBuffer);
		_uniform_buffers[key] = buffer;
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
					}
				}
			}
		}
	}
	return _uniform_buffers[key];
}
void ComputeShaderTask::_set_uniform_buffer(const int64_t binding, const int64_t set, const RID& buffer_rid) {
	const Vector2i key(binding, set);
	_uniform_buffers[key] = RDUniformBuffer::ref(buffer_rid);
}

void ComputeShaderTask::_dispatch(const int64_t kernel_index, const Vector3i thread_groups) {
	if (kernel_index < 0 || kernel_index >= kernels.size()) {
		UtilityFunctions::push_error(String("Attempted to dispatch invalid kernel index %s (max %s)!") % PackedStringArray({ String::num_int64(kernel_index), String::num_int64(kernels.size() - 1) }));
		return;
	}
	if (const Ref<ComputeShaderKernel> kernel = kernels[kernel_index]; kernel.is_null()) {
		UtilityFunctions::push_error(String("Attempted to dispatch invalid kernel index %s (found: nil)!") % PackedStringArray({ String::num_int64(kernel_index), String::num_int64(kernels.size() - 1) }));
		return;
	}
	RenderingDevice* rendering_device = RenderingServer::get_singleton()->get_rendering_device();
	if (!rendering_device) {
		UtilityFunctions::push_error("ComputeShaderTask: Couldn't obtain rendering device for dispatch!");
		return;
	}
	_update_buffers(kernel_index);
	const int64_t compute_list = rendering_device->compute_list_begin();
	const RID pipeline = _get_shader_pipeline_rid(kernel_index, rendering_device);
	rendering_device->compute_list_bind_compute_pipeline(compute_list, pipeline);
	_bind_uniform_sets(kernel_index, compute_list, rendering_device);
	rendering_device->compute_list_dispatch(compute_list, thread_groups.x, thread_groups.y, thread_groups.z);
	rendering_device->compute_list_end();
}
