# realistic_terrain_dx11_plugin
Shader plugin to calculate terrain erosion for use with [realistic_terrain](https://github.com/TLabAltoh/realistic_terrain)  

## Getting Started
### Requirements
- [pybind11](https://github.com/pybind/pybind11)
- [python-3.10.8](https://www.python.org/downloads/release/python-3108/)
- directX11
- windows10
- cmake (3.24.0)
### Install
Clone the repository to any directory with the following command  
```
git clone https://github.com/TLabAltoh/realistic_terrain_dx11_plugin
```
Install [pybind11](https://github.com/pybind/pybind11) and [python-3.10.8 (source release)](https://www.python.org/downloads/release/python-3108/) under the repository.  
### Build
1. Uninstall everything except python 3.10 series from windows
2. Edit environment variable ```PATH``` so that ```Python``` in ```System App``` takes precedence over ```Python``` in ```Blender```
3. Create ```./build``` then move to ```./build``` and execute the following command  
```
cmake ..
cmake --build .
```


