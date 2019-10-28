extends Camera

# Exports
export (float, 0.0, 1.0) var sensitivity :float = 0.5
export (float, 0.001, 0.999) var mouseSmoothness :float = 0.7
export (float, 0.0, 5.0) var speed :float = 3.5
export (float, 0.0, 360.0) var yawLimit :float = 360.0
export (float, 0.0, 360.0) var pitchLimit :float = 360.0

# Private vars
var _mousePos := Vector2(0.0, 0.0)
var _yaw :float = 0.0 
var _pitch :float = 0.0
var _totalYaw :float = 0.0
var _totalPitch :float = 0.0
var _movement := Vector3(0.0, 0.0, 0.0)

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	pass

# Handle input
func _input(event: InputEvent) -> void:
	# Facing
	if event is InputEventMouseMotion:
		_mousePos = event.relative
	
	# Movement
	if event is InputEventKey:
		if event.is_action("move_forward"):
			_movement.z = -1.0 if event.pressed else 0.0
		if event.is_action("move_back"):
			_movement.z = 1.0 if event.pressed else 0.0
		if event.is_action("strafe_left"):
			_movement.x = -1.0 if event.pressed else 0.0
		if event.is_action("strafe_right"):
			_movement.x = 1.0 if event.pressed else 0.0

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	# Update looking direction
	_mousePos *= sensitivity
	_yaw = _yaw * mouseSmoothness + _mousePos.x * (1.0 - mouseSmoothness)
	_pitch = _pitch * mouseSmoothness + _mousePos.y * (1.0 - mouseSmoothness)
	_mousePos = Vector2(0.0, 0.0)
	
	# Apply yaw & pitch limits
	if yawLimit < 360.0:
		_yaw = clamp(_yaw, -yawLimit - _totalYaw, yawLimit - _totalYaw)
	if pitchLimit < 360.0:
		_pitch = clamp(_pitch, -pitchLimit - _totalPitch, pitchLimit - _totalPitch)
	
	# Set final yaw & pitch values
	_totalYaw += _yaw
	_totalPitch += _pitch
	
	# Rotate
	rotate_y(deg2rad(-_yaw))
	rotate_object_local(Vector3(1,0,0), deg2rad(-_pitch))
	
	# Move
	var movement :Vector3 = _movement.normalized()
	movement *= speed
	translate(movement * delta)
	
