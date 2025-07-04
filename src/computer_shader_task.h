#pragma once

#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/typed_array.hpp"

#include "binding_macros.h"
#include "compute_shader_kernel.h"

using namespace godot;

class ComputeShaderTask : public RefCounted {
    GDCLASS(ComputeShaderTask, RefCounted);

    GET_SET_PROPERTY(TypedArray<ComputeShaderKernel>, kernels)

protected:
    static void _bind_methods();

    ComputeShaderTask() = default;
    ~ComputeShaderTask() override;

    Variant get_shader_parameter(const StringName& param) const;
    void set_shader_parameter(const StringName& param, const Variant& value);

    void dispatch_all(Vector3i thread_groups);
    void dispatch(const StringName& kernel_name, Vector3i thread_groups);
    void dispatch_at(int64_t kernel_index, Vector3i thread_groups);

private:
    Dictionary _shader_parameters{};
    Dictionary _kernel_shaders{};
    Dictionary _kernel_pipelines{};

    RID _get_shader_rid(int64_t kernel_index, RenderingDevice* rd);
    RID _get_shader_pipeline_rid(int64_t kernel_index, RenderingDevice* rd);

    void _bind_uniform_sets(int64_t kernel_index, int64_t compute_list, RenderingDevice* rd);
    void _dispatch(int64_t kernel_index, Vector3i thread_groups);
};
