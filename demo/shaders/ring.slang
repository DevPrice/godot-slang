// A simple shader ported from https://www.shadertoy.com/view/XdlSDs
// Author: dynamite
// License: CC BY-NC-SA 3.0 https://creativecommons.org/licenses/by-nc-sa/3.0/deed.en

import godot;

[gd::compositor::ColorTexture]
RWTexture2D<float4> scene_color;

[gd::Time]
float t;

[vk::push_constant]
[gd::compositor::Size]
int2 screen_size;

[shader("compute")]
[numthreads(8, 8, 1)]
void ring(uint2 thread_id: SV_DispatchThreadID) {
	float2 p = (thread_id.xy * 2.0f - screen_size.xy) / (float)screen_size.y;
	float tau = float.getPi() * 2.0;
	float a = atan2(p.x, p.y);
	float r = length(p) * 0.75;
	float2 uv = float2(a / tau, r);

	float x_col = (uv.x - t / 3) * 3.0;
	x_col = fmod(abs(x_col), 3.0f);
	float3 hor_colour = float3(0.25, 0.25, 0.25);

	if (x_col < 1.0) {
		hor_colour.r += 1.0 - x_col;
		hor_colour.g += x_col;
	} else if (x_col < 2.0) {
		x_col -= 1.0;
		hor_colour.g += 1.0 - x_col;
		hor_colour.b += x_col;
	} else {
		x_col -= 2.0;
		hor_colour.b += 1.0 - x_col;
		hor_colour.r += x_col;
	}

	uv = (2.0 * uv) - 1.0;
    float beam_width = (0.7 + 0.5 * cos(uv.x * 10.0 * tau * 0.15 * clamp(floor(5.0 + 10.0 * cos(t)), 0.0, 10.0))) * abs(1.0 / (30.0 * uv.y));
    float3 hor_beam = float3(beam_width);
    scene_color[thread_id.xy] = float4(((hor_beam)*hor_colour), 1.0);
}
