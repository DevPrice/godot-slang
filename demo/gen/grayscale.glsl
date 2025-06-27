#[compute]
#version 450
layout(row_major) uniform;
layout(row_major) buffer;

#line 1 0
layout(rgba32f)
layout(binding = 0)
uniform image2D scene_color_0;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{

#line 6
    ivec2 _S1 = ivec2(gl_GlobalInvocationID.xy);

#line 6
    vec4 _S2 = (imageLoad((scene_color_0), (_S1)));
    imageStore((scene_color_0), (_S1), vec4(_S2.x * 0.29899999499320984 + _S2.y * 0.58700001239776611 + _S2.z * 0.11400000005960464));
    return;
}

