#pragma once

#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/variant/typed_array.hpp"

#include "binding_macros.h"
#include "compute_shader_file.h"
#include "rdbuffer.h"

using namespace godot;

class ComputeShaderTask : public Resource {
	GDCLASS(ComputeShaderTask, Resource);

	GET_SET_PROPERTY(Ref<ComputeShaderFile>, shader)

protected:
	static void _bind_methods();

public:
	ComputeShaderTask();
	~ComputeShaderTask() override;

	[[nodiscard]] TypedArray<ComputeShaderKernel> get_kernels() const;

	[[nodiscard]] Variant get_shader_parameter(const StringName& param) const;
	void set_shader_parameter(const StringName& param, const Variant& value);

	void dispatch_all(Vector3i thread_groups);
	void dispatch(const StringName& kernel_name, Vector3i thread_groups);
	void dispatch_at(int64_t kernel_index, Vector3i thread_groups);

	[[nodiscard]] Dictionary get_shader_parameters() const;

	bool _set(const StringName& p_name, const Variant& p_value);
	bool _get(const StringName& p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo>* p_list) const;
	[[nodiscard]] bool _property_can_revert(const StringName& p_name) const;
	bool _property_get_revert(const StringName& p_name, Variant& r_property) const;

private:
	Dictionary _shader_parameters{};
	Dictionary _kernel_shaders{};
	Dictionary _kernel_pipelines{};
	Dictionary _buffers{};
	TypedArray<RID> _linear_sampler_cache{};
	TypedArray<RID> _nearest_sampler_cache{};
	PackedByteArray _push_constant{};

	void _reset();

	RID _get_shader_rid(int64_t kernel_index, RenderingDevice* rd);
	RID _get_shader_pipeline_rid(int64_t kernel_index, RenderingDevice* rd);

	[[nodiscard]] RID _get_sampler(RenderingDevice::SamplerFilter filter, RenderingDevice::SamplerRepeatMode repeat_mode) const;
	[[nodiscard]] Variant _get_parameter_value(const StringName& param_name, RenderingDevice::UniformType uniform_type, const Dictionary& attributes) const;
	[[nodiscard]] Variant _get_default_uniform(RenderingDevice::UniformType type, Dictionary user_attributes) const;
	Ref<RDBuffer> _get_buffer(int32_t binding, int32_t set);
	void _set_buffer(int32_t binding, int32_t set, const RID& buffer_rid);
	void _update_buffers(int64_t kernel_index);
	void _bind_uniform_sets(int64_t kernel_index, int64_t compute_list, RenderingDevice* rd);
	void _dispatch(int64_t kernel_index, Vector3i thread_groups);
};
