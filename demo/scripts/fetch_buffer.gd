extends Node

@export var task: ComputeShaderTask

func _ready() -> void:
	task.dispatch_all(Vector3i(1, 1, 1))
	var result := task.get_buffer_data("data")
	print(result.to_float32_array())
