extends Node3D

func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("toggle_data_mosh"):
		var data_mosh_effect: ComputeShaderEffect = load("res://example.tscn::ComputeShaderEffect_mt10o")
		data_mosh_effect.enabled = not data_mosh_effect.enabled
		data_mosh_effect.queue_dispatch("capture_screen")
		get_viewport().set_input_as_handled()
