#include "computer_shader_task.h"

#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/uniform_set_cache_rd.hpp"

void ComputeShaderTask::_bind_methods() {
    BIND_GET_SET_RESOURCE_ARRAY(ComputeShaderTask, kernels, ComputeShaderKernel)
    BIND_METHOD(ComputeShaderTask, get_shader_parameter, "param")
    BIND_METHOD(ComputeShaderTask, set_shader_parameter, "param", "value")
    BIND_METHOD(ComputeShaderTask, dispatch, "kernel_name", "thread_groups")
    BIND_METHOD(ComputeShaderTask, dispatch_at, "kernel_index", "thread_groups")
    BIND_METHOD(ComputeShaderTask, dispatch_all, "thread_groups")
}

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

RID ComputeShaderTask::_get_shader_rid(const int64_t kernel_index, RenderingDevice *rd) {
    if (!_kernel_shaders.has(kernel_index)) {
        const Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
        _kernel_shaders.set(kernel_index, rd->shader_create_from_spirv(kernel->get_spirv()));
    }
    return _kernel_shaders.get(kernel_index, RID{});
}

RID ComputeShaderTask::_get_shader_pipeline_rid(const int64_t kernel_index, RenderingDevice *rd) {
    if (!_kernel_pipelines.has(kernel_index)) {
        const RID shader_rid = _get_shader_rid(kernel_index, rd);
        _kernel_pipelines.set(kernel_index, rd->compute_pipeline_create(shader_rid));
    }
    return _kernel_pipelines.get(kernel_index, RID{});
}

RID ComputeShaderTask::_get_sampler(const RenderingDevice::SamplerFilter filter, const RenderingDevice::SamplerRepeatMode repeat_mode) const {
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
    return RID();
}

void ComputeShaderTask::_bind_uniform_sets(const int64_t kernel_index, const int64_t compute_list, RenderingDevice* rd) {
    Dictionary uniform_sets{};

    const Ref<ComputeShaderKernel> kernel = kernels[kernel_index];
    Dictionary parameters = kernel->get_parameters();
    Array parameter_keys = parameters.keys();
    for (uint32_t param_index = 0; param_index < parameter_keys.size(); param_index++) {
        const StringName& key = parameter_keys[param_index];
        const Dictionary param = parameters[key];
        const int64_t param_type = param.get("type", -1);
        const StringName param_name = param.get("name", StringName{});
        const int32_t binding_space = param.get("binding_space", 0);
        const int32_t binding_index = param.get("binding_index", 0);
        if (!param_name.is_empty() && param_type >= 0 && param_type < RenderingDevice::UniformType::UNIFORM_TYPE_MAX) {
            const auto uniform_type = static_cast<RenderingDevice::UniformType>(param_type);
            if (Variant value = _shader_parameters[param_name]; value.get_type() != Variant::NIL) {
                Ref uniform = memnew(RDUniform);
                uniform->set_binding(binding_index);
                uniform->set_uniform_type(uniform_type);
                if (value.get_type() == Variant::ARRAY) {
                    Array array = value;
                    for (size_t i = 0; i < array.size(); ++i) {
                        uniform->add_id(array[i]);
                    }
                } else {
                    uniform->add_id(value);
                }
                TypedArray<RDUniform> uniforms = uniform_sets.get_or_add(binding_space, TypedArray<RDUniform>{});
                uniforms.push_back(uniform);
            } else {
                Ref<RDUniform> uniform = _get_default_uniform(uniform_type, param["user_attributes"]);
                if (uniform.is_valid()) {
                    uniform->set_binding(binding_index);
                    uniform->set_uniform_type(uniform_type);
                    TypedArray<RDUniform> uniforms = uniform_sets.get_or_add(binding_space, TypedArray<RDUniform>{});
                    uniforms.push_back(uniform);
                }
            }
        }
    }

    Array uniform_set_keys = uniform_sets.keys();
    for (uint32_t i = 0; i < uniform_set_keys.size(); i++) {
        const uint32_t key = uniform_set_keys[i];
        const TypedArray<RDUniform> value = uniform_sets.get(key, TypedArray<RDUniform>{});
        RID shader_rid = _get_shader_rid(kernel_index, rd);
        RID uniform_set = UniformSetCacheRD::get_cache(shader_rid, key, value);
        rd->compute_list_bind_uniform_set(compute_list, uniform_set, key);
    }
}

Ref<RDUniform> ComputeShaderTask::_get_default_uniform(const RenderingDevice::UniformType type, Dictionary user_attributes) const {
    Ref uniform = memnew(RDUniform);
    uniform->set_uniform_type(type);
    if (user_attributes.has("gd_GlobalParam")) {
        const Dictionary attribute = user_attributes["gd_GlobalParam"];
        const String param_name = attribute["name"];
        // TODO: This doesn't work
        const RID resource_rid = RenderingServer::get_singleton()->global_shader_parameter_get(param_name);
        if (resource_rid.is_valid()) {
            uniform->add_id(resource_rid);
            return uniform;
        }
    }
    switch (type) {
        case RenderingDevice::UNIFORM_TYPE_SAMPLER:
            if (user_attributes.has("gd_LinearSampler")) {
                const Dictionary sampler_attribute = user_attributes["gd_LinearSampler"];
                int64_t repeat_mode_int = sampler_attribute.get("repeat_mode", -1);
                uniform->add_id(_get_sampler(RenderingDevice::SAMPLER_FILTER_LINEAR, static_cast<RenderingDevice::SamplerRepeatMode>(repeat_mode_int)));
                return uniform;
            }
            if (user_attributes.has("gd_NearestSampler")) {
                const Dictionary sampler_attribute = user_attributes["gd_NearestSampler"];
                int64_t repeat_mode_int = sampler_attribute.get("repeat_mode", 0);
                uniform->add_id(_get_sampler(RenderingDevice::SAMPLER_FILTER_NEAREST, static_cast<RenderingDevice::SamplerRepeatMode>(repeat_mode_int)));
                return uniform;
            }
            break;
        case RenderingDevice::UNIFORM_TYPE_TEXTURE:

            break;
        default: return nullptr;
    }
    return nullptr;
}

void ComputeShaderTask::_dispatch(const int64_t kernel_index, const Vector3i thread_groups) {
    if (kernel_index < 0 || kernel_index >= kernels.size()) {
        UtilityFunctions::push_error(String("Attempted to dispatch invalid kernel index %s (max %s)!") % PackedStringArray({String::num_int64(kernel_index), String::num_int64(kernels.size() - 1)}));
        return;
    }
    if (const Ref<ComputeShaderKernel> kernel = kernels[kernel_index]; kernel.is_null()) {
        UtilityFunctions::push_error(String("Attempted to dispatch invalid kernel index %s (found: nil)!") % PackedStringArray({String::num_int64(kernel_index), String::num_int64(kernels.size() - 1)}));
        return;
    }
    RenderingDevice* rendering_device = RenderingServer::get_singleton()->get_rendering_device();
    if (!rendering_device) {
        UtilityFunctions::push_error("ComputeShaderTask: Couldn't obtain rendering device for dispatch!");
        return;
    }
    const int64_t compute_list = rendering_device->compute_list_begin();
    const RID pipeline = _get_shader_pipeline_rid(kernel_index, rendering_device);
    rendering_device->compute_list_bind_compute_pipeline(compute_list, pipeline);
    _bind_uniform_sets(kernel_index, compute_list, rendering_device);
    rendering_device->compute_list_dispatch(compute_list, thread_groups.x, thread_groups.y, thread_groups.z);
    rendering_device->compute_list_end();
}
