# godotdetour
 GDNative plugin for the [Godot Engine](https://godotengine.org/) (3.1+) that implements [recastnavigation](https://github.com/recastnavigation/recastnavigation) - a fast and stable 3D navigation library using navigation meshes, agents, dynamic obstacles and crowds.  

### No editor integration
I wrote this plugin to be used in my own project, which has no use for an editor integration - it uses procedural generation of levels at runtime.  
As such, I have no need for any kind of editor integration or in-editor "baking" and did not implement it.  
That said, if anyone wants to add this, feel very free to make a pull request. It shouldn't be too much work.

Of course, godotdetour can still very much be used for projects using levels created in the editor itself.  
You merely have to pass the level's geometry to create a navmesh - which you can then save and package with your project, to be loaded when you load the level.

### Why not use Godot's own navigation?
For 2D, Godot's navigation might be serviceable, but for 3D, its navigation is lacking to the point of being entirely useless for 3D projects.  
You can merely get a path from A to B, but only if you bake the navigation mesh in the editor. Procedural generation is not covered at all. Maybe more importantly, it doesn't feature any concept of dynamic obstacles, agents, crowds or avoidance - all of which are reasons you want an actual navigation library and not just use Astar.

It was supposed to be reworked first in 2.X, then 3.0, 3.1, 3.2... currently, the goal is to have new navigation in 4.0. But there is no certainty of this actually happening in that version, and even if it does - will it have everything that [recastnavigation](https://github.com/recastnavigation/recastnavigation) can offer? Maybe. Maybe not.

I came to the conclusion that I had to roll my own navigation if I wanted to use Godot 3.2 for my own project, and decided to do it in a fashion that it might be of use to other people as well, hence this repository.

### How to build
Note that I build for linux 64bit release in this guide, but you can change those options, of course (check the [build system documentation](https://docs.godotengine.org/en/3.1/development/compiling/introduction_to_the_buildsystem.html)).  
I don't see a reason why this module wouldn't work on any other desktop platform supported by recast.

1. Check out the repo
2. Initialize the submodules:  

```
git submodule update --init --recursive
```

3. Build Godot's cpp bindings:  
```
cd godot-cpp
scons platform=linux generate_bindings=yes bits=64 target=release -j 4
cd ..
```
4. Build godotccd:
```
scons platform=linux target=release -j 4
```
The compiled gdnative module should now be under demo/godotdetour/bin.

### Demo/Example code
The demo showcases how to:
* Pass a mesh at runtime to godotdetour and create a navmesh from it
* Save, load and apply said navmesh
* Create and remove agents
* Set targets for agents to navigate to
* Enable/Disable debug rendering. Please note that the debug drawing only encompasses the navmesh itself, the agents and dynamic obstacles, not everything that the official RecastDemo offers.

Simply open the project under /demo. But don't forget to compile the module first.

### How to integrate godotdetour into your project
After building, copy the `demo/addons/godotdetour` folder to your own project, make sure to place it in an identical path, e.g. `<yourProject>/addons/godotdetour`. Otherwise, the paths inside the various files won't work.  
Of course, you are free to place it anywhere, but then you'll have to adjust the paths yourself.

Please also read [this guide](https://docs.godotengine.org/en/3.1/tutorials/plugins/gdnative/gdnative-cpp-example.html#using-the-gdnative-module) on how to use C++ GDNative modules.

### How to use
#### Initialization
In order to use this plugin, you only have to learn how to use four classes (three if you don't need dynamic obstacles).  

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
This is a very important and unfortunately rather lengthy step. Navigation is a complex topic with lots of parameters to fine-tune things, so there's not really a way around this. I tried my best to document each parameter in the C++ files (check the demo and headers, especially detournavigationmesh.h), but you might end up fiddling around with parameters until they work right for you anyway.  

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

In theory, you could set up each different navigation mesh completely different. However, the main purpose of having different navigation meshes is to have separate ones for different agent sizes. Changing more than the supported agent and cell sizes might lead to problems down the line.

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

#### Mark areas as water, grass, etc.

To mark areas of an already initialized navigation, you have to do the following:
```GDScript
```
These changes will not take effect until you tell the navigation to rebuild all changed tiles.  
This is done because rebuilding the tiles is a non-trivial operation so it is better to first mark all areas you need, and then call the rebuild:
```GDScript
```

Of course, the easiest way to achieve different ground types is to mark all of them prior to even initializing.  
Therefore, `markConvexArea()` is callable before `initialize()` so that the initialization can already mark everything correctly.

The currently supported area flags and their values are:  
`ground = 0, road = 1, water = 2, door = 3, grass = 4, jump = 5`

#### Create agents and giving them a target

#### Update your own objects with agent position & rotation

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

To show the debug drawing information of the agents, you need to call a similar function on the agent itself:
```GDScript
```
Note that this, too, is not updated after initial creation.

### Hints

### Missing Features/TODOs
The following features (from recast/detour or otherwise) are not yet part of godotdetour.  
They might be added by someone else down the line, or by myself once I need them for my own project:  
* Off-mesh connections/portals.
* More debug rendering.
* Better control over threading. For now, every new() instance of DetourNavigation will create its own thread.
* More control over which agent goes to which navigation mesh. Currently, only the agent radius & height is used automatically to determine this.
* Changing the navmesh after creation (by adding/removing level geometry that isn't just obstacles/marked areas)
* Support dynamic area flags instead of hard coded grass, water, etc.
