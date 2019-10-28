extends Spatial

const DetourNavigation 	            :NativeScript = preload("res://addons/godotdetour/detournavigation.gdns")
const DetourNavigationParameters	:NativeScript = preload("res://addons/godotdetour/detournavigationparameters.gdns")
const DetourNavigationMesh 	        :NativeScript = preload("res://addons/godotdetour/detournavigationmesh.gdns")
const DetourNavigationMeshParameters    :NativeScript = preload("res://addons/godotdetour/detournavigationmeshparameters.gdns")
const DetourCrowdAgent	            :NativeScript = preload("res://addons/godotdetour/detourcrowdagent.gdns")
const DetourCrowdAgentParameters    :NativeScript = preload("res://addons/godotdetour/detourcrowdagentparameters.gdns")
const DetourObstacle				:NativeScript = preload("res://addons/godotdetour/detourobstacle.gdns")

var navigation = null
var testIndex :int = -1
onready var nextStepLbl : RichTextLabel = get_node("Control/NextStepLbl")

# Called when the node enters the scene tree for the first time.
func _ready():
	# Create the detour navigation
	pass
	
# Called when the user presses a key
func _input(event :InputEvent) -> void:
	# Quit the application
	if event.is_action("ui_cancel") && event.is_pressed():
		get_tree().quit()
	# Start the next test
	if event.is_action("ui_select") && event.is_pressed():
		testIndex += 1
		doNextTest(testIndex)

# Do the next test in line
func doNextTest(index :int) -> void:
	if index == 0:
		nextStepLbl.text = "Initializing the navigation..."
		yield(get_tree(), "idle_frame")
		initializeNavigation()
		nextStepLbl.text = "Next step:      Enable Navigation Debug Drawing"
		
# Initializes the navigation
func initializeNavigation():
	# Create the navigation parameters
	var navParams = DetourNavigationParameters.new()
	navParams.ticksPerSecond = 60 # How often the navigation is updated per second in its own thread
	navParams.maxObstacles = 256 # How many dynamic obstacles can be present at the same time
	
	# Create the parameters for the "small" navmesh
	var navMeshParamsSmall = DetourNavigationMeshParameters.new()
	# It is important to understand that recast/detour operates on a voxel field internally.
	# The size of a single voxel (often called cell internally) has significant influence on how a navigation mesh is created.
	# A tile is a rectangular region within the navigation mesh. In other words, every navmesh is divided into equal-sized tiles, which are in turn divided into cells.
	# The detail mesh is a mesh used for determining surface height on the polygons of the navigation mesh.
	# Units are usually in world units [wu] (e.g. meters, or whatever you use), but some may be in voxel units [vx] (multiples of cellSize).
	
	# x = width & depth of a single cell (only one value as both must be the same) | y = height of a single cell. [wu]
	navMeshParamsSmall.cellSize = Vector2(0.3, 0.2)
	# How steep an angle can be to still be considered walkable. In degree. Max 90.0.
	navMeshParamsSmall.maxAgentSlope = 45.0
	# The maximum height of an agent supported in this navigation mesh. [wu]
	navMeshParamsSmall.maxAgentHeight = 2.0
	# How high a single "stair" can be to be considered walkable by an agent. [wu]
	navMeshParamsSmall.maxAgentClimb = 0.3
	# The maximum radius of an agent in this navigation mesh. [wu]
	navMeshParamsSmall.maxAgentRadius = 1.0
	# The maximum allowed length for contour edges along the border of the mesh. [wu]
	navMeshParamsSmall.maxEdgeLength = 12.0
	# The maximum distance a simplified contour's border edges should deviate the original raw contour. [vx]
	navMeshParamsSmall.maxSimplificationError = 1.3
	# How many cells an isolated area must at least have to become part of the navmesh.
	navMeshParamsSmall.minNumCellsPerIsland = 8
	# Any regions with a span count smaller than this value will, if possible, be merged with larger regions.
	navMeshParamsSmall.minCellSpanCount = 20
	# Maximum number of vertices per polygon in the navigation mesh.
	navMeshParamsSmall.maxVertsPerPoly = 6
	# The width,depth & height of a single tile. [vx]
	navMeshParamsSmall.tileSize = 42
	# How many vertical layers a single tile is expected to have. Should be less for "flat" levels, more for something like tall, multi-floored buildings.
	navMeshParamsSmall.layersPerTile = 4
	# The sampling distance to use when generating the detail mesh. [wu]
	navMeshParamsSmall.detailSampleDistance = 6.0
	# The maximum allowed distance the detail mesh should deviate from the source data. [wu]
	navMeshParamsSmall.detailSampleMaxError = 1.0
	navParams.navMeshParameters.append(navMeshParamsSmall)
	
	# Create the parameters for the "large" navmesh
	var navMeshParamsLarge = DetourNavigationMeshParameters.new()
	navMeshParamsLarge.cellSize = Vector2(0.5, 0.35)
	navMeshParamsLarge.maxAgentSlope = 45.0
	navMeshParamsLarge.maxAgentHeight = 4.0
	navMeshParamsLarge.maxAgentClimb = 0.5
	navMeshParamsLarge.maxAgentRadius = 2.5
	navMeshParamsLarge.maxEdgeLength = 12.0
	navMeshParamsLarge.maxSimplificationError = 1.3
	navMeshParamsLarge.minNumCellsPerIsland = 8
	navMeshParamsLarge.minCellSpanCount = 20
	navMeshParamsLarge.maxVertsPerPoly = 6
	navMeshParamsLarge.tileSize = 42
	navMeshParamsLarge.layersPerTile = 4
	navMeshParamsLarge.detailSampleDistance = 6.0
	navMeshParamsLarge.detailSampleMaxError = 1.0
	navParams.navMeshParameters.append(navMeshParamsLarge)
	
	# Create the arrayMesh from the CSG and set it as the meshInstance's mesh
	var csgCombiner :CSGCombiner = get_node("CSGCombiner")
	csgCombiner._update_shape()
	var arrayMesh :ArrayMesh = csgCombiner.get_meshes()[1]
	var meshInstance :MeshInstance = get_node("MeshInstance")
	meshInstance.mesh = arrayMesh
	remove_child(csgCombiner)
	
	# Initialize the navigation with the mesh instance and the parameters
	navigation = DetourNavigation.new()
	navigation.initialize(meshInstance, navParams)
