# VEXWorkshop
Graphics API / Gamedev
Currently a set of half backed webgpu demos.
This is enviroment when I experiment with different things using SDL window / WEBGPU renderer as a fundation together with a small custom library (vexlib).

## Running samples
After cloning repo, run 
```
git submodule update --init
```
Then install packages needed by Dawn and make sure python is in PATH:
```
python where 
^--- run to check if python can be found 
pip install MarkupSafe==2.0.0
^--- install package needed by dawn (newer versions of MarkupSafe fail)
```
Then use CMake presets to configure and run VEXWorkshop target. For now main three configurations to use are ClangDawn Debug/ReleaseDbgInfo and Clang Web Build (should be run from cmd with emcmake).

If you are using Visual Studio the best way to work with project is to open folder with it and use CMake built-in support.

[issue] If configuration fails with "X target is not found" error, simply run it again.