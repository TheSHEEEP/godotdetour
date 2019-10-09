# godotdetour
 GDNative plugin for the [Godot Engine](https://godotengine.org/) (3.1+) that implements [recastnavigation](https://github.com/recastnavigation/recastnavigation) - a fast and stable 3D navigation library using navigation meshes, agents, dynamic obstacles and crowds.  

### No editor integration?
I wrote this plugin to be used in my own project, which has no use for an editor integration - it uses procedural generation of levels at runtime.  
As such, I have no need for any kind of editor integration or in-editor "baking" and did not implement it.  
That said, if anyone wants to add this, feel very free to make a pull request.

Of course, godotdetour can still very much be used for projects using levels created in the editor itself.  
You merely have to pass the level's geometry to create a navmesh - which you can then save and package with your project, to be loaded when you load the level.

### Why not use Godot's own navigation?
For 2D, Godot's navigation might be serviceable, but for 3D, its navigation is lacking to the point of being entirely useless for 3D projects.  
You can merely get a path from A to B, but only if you bake the navigation mesh in the editor. Procedural generation is not covered at all. Maybe more importantly, it doesn't feature any concept of dynamic obstacles, agents, crowds or avoidance.  
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
scons platforms=linux target=release -j 4
```
The compiled gdnative module should now be under demo/godotdetour/bin.

### Demo/Example code
The demo showcases how to:
* Pass a mesh at runtime to godotdetour and create a navmesh from it
* Save, load and apply said navmesh
* Create and remove agents
* Set targets for agents to navigate to
* Enable/Disable debug rendering

Simply open the project under /demo. But don't forget to compile the module first.

### How to integrate godotdetour into your project
After building, copy the `demo/addons/godotdetour` folder to your own project, make sure to place it in an identical path, e.g. `<yourProject>/addons/godotdetour`. Otherwise, the paths inside the various files won't work.  
Of course, you are free to place it anywhere, but then you'll have to adjust the paths yourself.

Please also read [this guide](https://docs.godotengine.org/en/3.1/tutorials/plugins/gdnative/gdnative-cpp-example.html#using-the-gdnative-module) on how to use C++ GDNative modules.

### How to use


### Demo/Example code


### Hints
