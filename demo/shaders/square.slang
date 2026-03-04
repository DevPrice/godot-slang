import godot;

[gd::Export]
let data : RWStructuredBuffer<float>;

[shader("compute")]
[numthreads(8, 1, 1)]
void square(uint3 threadId: SV_DispatchThreadID) {
    data[threadId.x] = data[threadId.x] * data[threadId.x];
}
