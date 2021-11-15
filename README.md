# godotdetour
 GDNative plugin for the [Godot Engine](https://godotengine.org/) (3.4+) that implements [recastnavigation](https://github.com/recastnavigation/recastnavigation) - a fast and stable 3D navigation library using navigation meshes, agents, dynamic obstacles and crowds.  

![demo2](https://media.giphy.com/media/YT8OWY266iqbGRNeWc/source.gif)

**Status:** Maintenance Mode  

Since I needed to move on to other preparations for my project, I do not currently have the time to actively work on godotdetour a lot. At least until the time comes to integrate godotdetour into said project.  
That said, I will try to fix reported bugs and definitely take a look at pull requests (either for new features or fixes), just be aware it might take a while.

Currently, not too much testing was done beyond the demo, so do expect a bug or two. ;)

### Notable features
* Creation of navmeshes at runtime
* Navigation agents with avoidance behavior
* Multiple navmeshes at the same time for the same geometry (e.g. one for small, one for large agents)
* Marking areas as grass, water, etc. and configurations for agents to treat those differently
* Agent prediction
* Off-mesh connections (aka portals) \*\*
* Basic debug rendering
* Temporary obstacles
* Runs in its own thread
* Highly configurable

### Missing features/TODOs
The following features are not (yet) part of godotdetour.  
They might be added by someone else down the line, or by myself once I need them for my own project:  
* More debug rendering
* Better control over threading. For now, every new() instance of DetourNavigation will create its own thread
* More control over which agent goes to which navigation mesh. Currently, only the agent radius & height is used automatically to determine this
* Changing the navmesh after creation (by adding/removing level geometry that isn't just obstacles/marked areas/off-mesh connections) without requiring a full reload
* Support dynamic area flags instead of hard coded grass, water, etc
* Various other optimizations and ease-of-use functions

### No editor integration
I wrote this plugin to be used in my own project, which has no use for an editor integration as it uses procedural generation of levels at runtime.  
That said, if anyone wants to add this, feel very free to make a pull request.

Of course, godotdetour can still very much be used for projects using levels created in the editor itself.  
You only have to pass the level's geometry to create a navmesh - which you can then save and package with your project, to be loaded when you load the level.

### Comparison with Godot 3.2's Navigation
For 2D, Godot 3.2's navigation might be serviceable, but for 3D, its navigation is lacking to the point of being entirely useless.  
You can merely get a path from A to B, but only if you bake the navigation mesh in the editor. Procedural generation is not covered at all. Maybe more importantly, it doesn't feature any concept of dynamic obstacles, agents, crowds or avoidance - all of which are reasons you want an actual navigation library and not just use Astar.

I came to the conclusion that I had to roll my own navigation if I wanted to use Godot 3.2 for my own project, and decided to do it in a fashion that it might be of use to other people as well, hence this repository.

### Comparison with Godot 4.0's NavigationServer
Both projects seem to have different foci and goals:
- NavigationServer is fully integrated into Godot. Godotdetour was always meant to be used as a module. Which also allows easy modifications on the c++ side without having to rebuild Godot itself.
- NavigationServer does not support Godot 3.2 - godotdetour is built for 3.2 (though I will possibly "port" it to 4.0 if I ever switch my own project to it as well).
- NavigationServer is very high-level, with lots of the internal recast and RVO2 configurations hidden away from the user to make things easier. godotdetour is more low-level and exposes most if not all of the little screws to GDScript so users can (and must) fine-tune the resulting navigation meshes and agent behaviors to their needs.
- godotdetour has built-in support for multiple navmeshes at the same time (eg. one for smaller agents, one for larger) and manages those automatically. Same with marking areas as grass, road, water, etc.
- godotdetour uses the recast + detour library, while NavigationServer uses recast for building navmeshes and then uses RVO2 for collision avoidance.

### How to build
Note that I build for linux 64bit release in this guide, but you can change those options, of course (check the [build system documentation](https://docs.godotengine.org/en/3.1/development/compiling/introduction_to_the_buildsystem.html)).  
I don't see a reason why this module wouldn't work on any other desktop platform supported by recast.

1. Check out the repo
2. Initialize the submodules:  

```
git submodule update --init --recursive
```

2b. **As of June 29th, 2020, this should no longer be necessary**  
~~ (Windows only) There is a godot-cpp bug affecting Windows that isn't fixed yet in master and requires you to apply a pull request locally. Hopefully, this will be merged at some point and this step will become unnecessary: ~~

```
cd godot-cpp
git fetch origin pull/346/head:WinFix
git checkout WinFix
cd ..
```

3. Build Godot's cpp bindings:  
```
cd godot-cpp
scons platform=linux generate_bindings=yes bits=64 target=release -j 4
cd ..
```

4. Build godotdetour:
```
scons platform=linux target=release -j 4
```
The compiled gdnative module should now be under demo/godotdetour/bin.

### Demo/Example code
The demo showcases how to:
* Pass a mesh at runtime to godotdetour and create a navmesh from it
* Save, load and apply said navmesh
* Create and remove agents and temporary obstacles
* Set targets for agents to navigate to
* Mark areas as grass/water, etc. and rebuild the navmesh at runtime
* Agent prediction
* Off-mesh connections
* Debug rendering. Please note that the debug drawing only encompasses the navmesh itself, marked areas and dynamic obstacles, not everything that the official RecastDemo offers.

Simply open the project under /demo. But don't forget to compile the module first.

### How to integrate godotdetour into your project
After building, copy the `demo/addons/godotdetour` folder to your own project, make sure to place it in an identical path, e.g. `<yourProject>/addons/godotdetour`. Otherwise, the paths inside the various files won't work.  
Of course, you are free to place it anywhere, but then you'll have to adjust the paths yourself.

Please also read [this guide](https://docs.godotengine.org/en/3.1/tutorials/plugins/gdnative/gdnative-cpp-example.html#using-the-gdnative-module) on how to use C++ GDNative modules.

### How to use
#### Initialization
First of all, make sure the native scripts are loaded:
```GDScript
const DetourNavigation 	            :NativeScript = preload("res://addons/godotdetour/detournavigation.gdns")
const DetourNavigationParameters	:NativeScript = preload("res://addons/godotdetour/detournavigationparameters.gdns")
const DetourNavigationMesh 	        :NativeScript = preload("res://addons/godotdetour/detournavigationmesh.gdns")
const DetourNavigationMeshParameters    :NativeScript = preload("res://addons/godotdetour/detournavigationmeshparameters.gdns")
const DetourCrowdAgent	            :NativeScript = preload("res://addons/godotdetour/detourcrowdagent.gdns")
const DetourCrowdAgentParameters    :NativeScript = preload("res://addons/godotdetour/detourcrowdagentparameters.gdns")
```

Now, you need to initialize the Navigation.  
This is a very important and unfortunately rather lengthy step. Navigation is a complex topic with lots of parameters to fine-tune, so there's not really a way around this. I tried my best to document each parameter in the C++ files (check the demo and headers, especially detournavigationmesh.h) as well as the demo, but you might end up fiddling around with parameters until they work right for you anyway.  

Note that you can initialize different navigation meshes at the same time - the main purpose of this is to support different sized agents (e.g. one navigation mesh for every agent up to human size, another navigation mesh for every agent up to tank size). Don't add too many, though, as each extra navmesh adds significant calculation costs.  
Yes, this does blow up the initialization code somewhat, but at least you only have to do it once and can then stop worrying about it as this plugin manages the assigning of obstacles and agents automatically.
```GDScript
# Create the navigation parameters
var navParams = DetourNavigationParameters.new()
navParams.ticksPerSecond = 60 # How often the navigation is updated per second in its own thread
navParams.maxObstacles = 256 # How many dynamic obstacles can be present at the same time

# Create the parameters for the "small" navmesh
var navMeshParamsSmall = DetourNavigationMeshParameters.new()
navMeshParamsSmall.cellSize = Vector2(0.3, 0.2)
navMeshParamsSmall.maxNumAgents = 256
navMeshParamsSmall.maxAgentSlope = 45.0
navMeshParamsSmall.maxAgentHeight = 2.0
navMeshParamsSmall.maxAgentClimb = 0.3
navMeshParamsSmall.maxAgentRadius = 1.0
navMeshParamsSmall.maxEdgeLength = 12.0
navMeshParamsSmall.maxSimplificationError = 1.3
navMeshParamsSmall.minNumCellsPerIsland = 8
navMeshParamsSmall.minCellSpanCount = 20
navMeshParamsSmall.maxVertsPerPoly = 6
navMeshParamsSmall.tileSize = 42
navMeshParamsSmall.layersPerTile = 4
navMeshParamsSmall.detailSampleDistance = 6.0
navMeshParamsSmall.detailSampleMaxError = 1.0
navParams.navMeshParameters.append(navMeshParamsSmall)

# Create the parameters for the "large" navmesh
var navMeshParamsLarge = DetourNavigationMeshParameters.new()
navMeshParamsLarge.cellSize = Vector2(0.5, 0.35)
navMeshParamsLarge.maxNumAgents = 128
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

# Initialize the navigation with the mesh instance and the parameters
var navigation = DetourNavigation.new()
navigation.initialize(meshInstance, navParams)
```

In theory, you could set up each different navigation mesh completely different. However, the main purpose of having different navigation meshes is to have separate ones for different agent sizes. Changing more than the supported agent number and agent+cell sizes might lead to problems down the line.

#### Create, move and destroy temporary obstacles
To create a temporary obstacle, simply do the following:  
```GDScript
# Cylinder
var godotDetourObstacle = navigation.addCylinderObstacle(position, radius, height)
# Box
var godotDetourObstacle = navigation.addBoxObstacle(position, dimensions, rotationRad)
```

Moving and destroying an obstacle is equally easy:
```GDScript
godotDetourObstacle.move(Vector3(1.0, 2.0, 3.0))
godotDetourObstacle.destroy() # Don't forget to do this or you'll get a memory leak
```
**Important:** Any such change (creation, moving, destroying) will not take effect immediately, but instead after the next tick of the navigation thread.

Every obstacle also has two properties in GDScript:  
`position` - The obstacle's position (Vector3)  
`dimensions` - For boxes, these are the xyz-dimensions, for cylinders x = radius, y = height, z = unused.

#### Mark areas as water, grass, etc.
To mark areas of a navigation mesh as water/grass/etc. , you have to do the following:
```GDScript
# Create a bunch of vertices to form the bottom of a convex volume
var vertices :Array = []
vertices.append(Vector3(-2.0, -0.5, 1.7))
vertices.append(Vector3(3.2, -0.5, 2.2))
vertices.append(Vector3(2.3, -0.5, -2.0))
vertices.append(Vector3(-1.2, -0.5, -3.1))
# 1.5 is the height, 4 the area flag
var markerId = navigation.markConvexArea(vertices, 1.5, 4)
```
The ID returned by `markConvexArea` is the ID of the created convex volume marker.  
If -1 is returned, there was an error.

To remove a convex volume marker, simply call:
```GDScript
navigation.removeConvexAreaMarker(markerId)
```

None of these changes will take effect until you tell the navigation to rebuild all changed tiles.  
This is done because rebuilding the tiles is a non-trivial operation so it is better to first mark/unmark all areas you need, and then call the rebuild:
```GDScript
navigation.rebuildChangedTiles()
```

Of course, the easiest way to achieve different ground types is to mark all of them prior to even initializing.  
Therefore, `markConvexArea()` is callable before `initialize()` so that the initialization can already apply everything correctly.  
Remember that the default area type is ground, so there's no need to mark anything as that area type.

The currently supported area flags and their values are:  
`ground = 0, road = 1, water = 2, door = 3, grass = 4, jump = 5`

#### Query filters
Before adding any agents, you must create the query filters they will use.  
The filter fine-tunes an agent's pathfinding behavior by setting weights for certain area types, e.g. you can have a filter that makes walking on water impossible while walking on grass is preferred, or a filter that makes every area type have the same weight, etc.  
Recast/Detour supports up to 16 filters at the same time. If you need more, you'll have to change the recast/detour sources and rebuild.

To set a query filter, call the following function:
```GDScript
var weights :Dictionary = {}
weights[0] = 10.0       # Ground
weights[1] = 5.0        # Road
weights[2] = 10001.0    # Water
weights[3] = 10.0       # Door
weights[4] = 100.0      # Grass
weights[5] = 150.0      # Jump
navigation.setQueryFilter(0, "default", weights)
```
The name is what you will be using when creating an agent to refer to the filter.

Setting a weight of over 10000.0 will make that area completely impassible. Note that this will lump together ground, road and grass - so if either of them is set to a weight of over 10000.0, all of them will become impassible.

**Important:** At least one query filter must be set this way before creating an agent.

#### Off-mesh connections / portals
Godotdetour supports off-mesh connections (aka portals).  
These are single- or bi-directional connections between a point on a navmesh and another point - with bi-directional points, both have to be on the navmesh.  
You can see an example of this in the demo.

To create an off-mesh connection, call the following function:
```GDScript
start = Vector3(0.0, 0.0, 0.0)
end = Vector3(1.0, 12.0, 123.0)
bidirectional = true    # If the connection can be entered from both points or only the start point
radius = 0.35           # The radius around the start/end points defining the portal zone   
areaType = 0            # Ground, grass, etc.
offMeshID = navigation.addOffMeshConnection(start, end, bidirectional, radius, areaType)
```

To remove an off-mesh connection:
```GDScript
navigation.removeOffMeshConnection(offMeshID)
```

Just like area marking, none of these changes will take effect until a `rebuildChangedTiles()` has been called. Likewise, off-mesh connections can be added before initializing the navigation.

\*\* **IMPORTANT**: Off-mesh connections work only when they are relatively close together. This is a restriction of the underlying recast library.

#### Create/delete agents and give them a target
Creating an agent is done via the navigation object:
```GDScript
# Create an agent in GodotDetour and remember both
var params = DetourCrowdAgentParameters.new()
params.position = Vector3(0.0, 0.0, 0.0)
params.radius = 0.7
params.height = 1.6
params.maxAcceleration = 6.0
params.maxSpeed = 3.0
params.filterName = "default"
params.anticipateTurns = true
params.optimizeVisibility = true
params.optimizeTopology = true
params.avoidObstacles = true
params.avoidOtherAgents = true
params.obstacleAvoidance = 1
params.separationWeight = 1.0
var detourCrowdAgent = navigation.addAgent(params)
```
Once you have the DetourCrowdAgent instance, you can use it to request new movement targets or stop it:
```GDScript
detourCrowdAgent.moveTowards(Vector3(10.0, 0.0, 10.0))

# To stop any movement (without removing the agent entirely)
detourCrowdAgent.stop()
```
**Important:** The agents will not start moving immediately, but instead during the next tick of the navigation thread.

To remove an agent:
```GDScript
navigation.removeAgent(detourCrowdAgent)
detourCrowdAgent = null
```

#### Update your own objects with agent position & velocity
To make any use of the pathfinding, you will have to apply agents' positions/velocities to your own objects:
```GDScript
# This should be done in any regularly called update-like function
yourOwnObject.translation = detourCrowdAgent.position

# Velocity can be used as a "look-at" direction
yourOwnObject.look_at(yourOwnObject.translation + detourCrowdAgent.velocity, agent.transform.basis.y)
```
As you can see, "velocity" can be used as a look-at/direction in a pinch.  
However, bear in mind that detour does not really have a concept of facing directions for agents. You will have to roll your own facing calculations if your objects need to face a different direction than the one they are walking towards.

**Important:** The values you get from the detourCrowdAgent object are always "outdated" by up to one navigation thread tick. Predicted values might be implemented at a later point.

#### Agent prediction
godotdetour offers a function in the DetourCrowdAgent class that can be used for predicting movement:  
```GDScript
# In a regularly called update function...
var result :Dictionary = detourCrowdAgent.getPredictedMovement(gameObject.translation, -gameObject.global_transform().basis.z, lastUpdateTimestamp, deg2rad(5))
gameObject.translation = result["position"]
gameObject.look_at(gameObject.translation + result["direction"], gameObject.transform.basis.y)

lastUpdateTimestamp = OS.get_ticks_msec()
```
This function calculates what the current position and direction of an external object should be, based on the passed object position and direction as well as the age of those values and of course the internal agent values.  
The last value passed to the function is the maximum amount of radians the resulting direction can differ from the input direction - use this avoid too sudden turning.  
**Important:** Calling this function is of course more costly than just applying the agent's position and velocity. It is up to you to decide if/when you want to use prediction.

#### Show debug mesh
Showing the debug drawing information of the navigation (shows navmesh, cache boundaries and temporary obstacles) is quite simple:  
```GDScript
# Create the debug mesh
# First parameter is the index of the navigation mesh (in case there are multiple)
# Second parameter is if you want to display the cache boundaries or not (false will only display the navmesh itself, which is likely what most want)
var debugMeshInstance :MeshInstance = navigation.createDebugMesh(0, false)

# Add the debug mesh instance a little elevated to avoid z-fighting
debugMeshInstance.translation = Vector3(0.0, 0.05, 0.0)
add_child(debugMeshInstance)
```
Please note that these meshes are **not updated** after initial creation. It is merely a snapshot of the current state.

#### Signals
godotdetour classes emit a number of signals that you can connect to.  
**Important:** The majority of signals are not emitted from the main thread, so you should connect to these signals deferred if that is an issue.

The `DetourNavigation` object emits the following signals:  
- `navigation_tick_done` - Emitted after each finished navigation thread tick. It has one parameter, the time the tick took, in milliseconds

The `DetourCrowdAgent` emits the following signals:  
- `arrived_at_target` - Emitted when the agent arrived at its target. Has one parameter, the agent itself.
- `no_progress` - Emitted when no noticeable progress (5% of maxSpeed) has been made towards the target in five seconds. Has two parameters, the agent itself and the distance left to the target.
- `no_movement` - Emitted when the agent has not moved (2.5% of maxSpeed) in one second. Has two parameters, the agent itself and the distance left to the target.

The `no_xxx` signals can be used to detect stuck agents, but be aware that they are not guarantees. If you want to avoid false "positives", it is recommended to await more than one of the signals and build your own stuck detection around them.
