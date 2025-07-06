#pragma once

#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/typed_array.hpp"

#include "binding_macros.h"
#include "compute_shader_kernel.h"

using namespace godot;

class RDUniformBuffer : public RefCounted {
	GDCLASS(RDUniformBuffer, RefCounted);

	GET_SET_PROPERTY(PackedByteArray, buffer)
	GET_SET_PROPERTY(RID, rid)

protected:
	static void _bind_methods();

public:
	RDUniformBuffer() = default;
	~RDUniformBuffer() override;

	void write(int64_t offset, int64_t size, const Variant& data);
};

class ComputeShaderTask : public RefCounted {
	GDCLASS(ComputeShaderTask, RefCounted);

	GET_SET_PROPERTY(TypedArray<ComputeShaderKernel>, kernels)

public:
	ComputeShaderTask();
	~ComputeShaderTask() override;

protected:
	static void _bind_methods();

	Variant get_shader_parameter(const StringName& param) const;
	void set_shader_parameter(const StringName& param, const Variant& value);

	void dispatch_all(Vector3i thread_groups);
	void dispatch(const StringName& kernel_name, Vector3i thread_groups);
	void dispatch_at(int64_t kernel_index, Vector3i thread_groups);

private:
	Dictionary _shader_parameters{};
	Dictionary _kernel_shaders{};
	Dictionary _kernel_pipelines{};
	Dictionary _uniform_buffers{};
	TypedArray<RID> _linear_sampler_cache{};
	TypedArray<RID> _nearest_sampler_cache{};

	RID _get_shader_rid(int64_t kernel_index, RenderingDevice* rd);
	RID _get_shader_pipeline_rid(int64_t kernel_index, RenderingDevice* rd);

	RID _get_sampler(RenderingDevice::SamplerFilter filter, RenderingDevice::SamplerRepeatMode repeat_mode) const;
	Variant _get_default_uniform(RenderingDevice::UniformType type, Dictionary user_attributes) const;
	Ref<RDUniformBuffer> _get_uniform_buffer(int64_t binding, int64_t set);
	void _update_buffers(int64_t kernel_index);
	void _bind_uniform_sets(int64_t kernel_index, int64_t compute_list, RenderingDevice* rd);
	void _dispatch(int64_t kernel_index, Vector3i thread_groups);
};
