extends Node

@export var task: ComputeShaderTask

func _ready() -> void:
	task.rendering_device = RenderingServer.create_local_rendering_device()
	task.dispatch_all(Vector3i(1, 1, 1))
	var result := task.get_buffer_data("data")
	print(result.to_float32_array())
