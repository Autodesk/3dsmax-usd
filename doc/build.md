





# Building

## Getting and Building the Code
### Download the source code

Start by cloning the repository:
```
git clone https://github.com/Autodesk/3dsmax-usd.git
cd 3dsmax-usd
```
##### Repository Layout
| Location | Description |
|--|--|
| additional_includes | Additional include files required to build the component (from 3ds Max restricted SDK) |
| samples | 3dsmax-usd sample plugins exposing its extensibility feature |
| src | The source folder for the 3dsmax-usd component |

### Dependencies
Multiple dependencies are required in order to build the component. Take the time to first set up your build environment properly.
* [System Prerequisites](#system-prerequisites)
* [OpenUSD](#download-and-build-openusd)
* [UFE](#universal-front-end-ufe)
* [Python](#python-modules)
* [MaterialX](#materialx-plugin-for-3ds-max)
* [Other dependencies](#other-dependencies)

#### 1. System Prerequisites

Before building the project, consult the [3ds Max SDK requirements](https://help.autodesk.com/view/MAXDEV/2025/ENU/?guid=sdk_requirements) to ensure you use the recommended version of compiler, operating system and Qt version. You will need to need a copy of the [3ds Max SDK](https://aps.autodesk.com/developer/overview/3ds-max) headers and libraries as well (direct link to [3ds Max 2025](https://autodesk-adn-transfer.s3.us-west-2.amazonaws.com/ADN+Extranet/M%26E/Max/Autodesk+3ds+Max+2025/SDK_3dsMax2025.msi) and [3ds Max 2024](https://autodesk-adn-transfer.s3.us-west-2.amazonaws.com/ADN+Extranet/M%26E/Max/Autodesk+3ds+Max+2024/SDK_3dsMax2024.msi)).

> For 3ds Max 2025, we suggest to use Microsoft Visual Studio 2019 (Community or better), version 16.10.4, C++ Platform Toolset v142, Windows Platform SDK 10.0.19041.0. 

Consult the [3ds Max Python API](https://help.autodesk.com/view/MAXDEV/2025/ENU/?guid=MAXDEV_Python_what_s_new_in_3ds_max_python_api_html) reference to learn more on the Python version to use.

> The Microsoft Visual Studio extension 'Qt VS Tools' must be installed and properly configured with the Qt version used by 3ds Max. Make sure to reference the selected *version* name in '*Extensions->Qt VS Tools->Qt Versions*'.
>
> 3ds Max Qt versions can be found on the https://github.com/autodesk-forks/qt5/releases. These versions contain modifications to the Qt codebase which are specific to 3ds Max. No need to build your own version if you are not making changes to Qt.

<br>

##### Simplified dependencies setup using the `devkit`
Fetching all dependencies can be long and tedious. A `devkit` archive is made available inside the 3ds Max USD plugin installation. The `devkit` includes all the 3ds Max USD SDK files (includes and libs), the samples and the minimal dependencies required to compile the samples (or your extensibility plugin to the 3ds Max USD plugin). Additionally, the `devkit` contains additional dependencies required to compile the full 3ds Max USD plugin.

The `devkit` can be found in the folder `Content` of an installed 3ds Max USD plugin (https://help.autodesk.com/view/3DSMAX/2025/ENU/?guid=GUID-1A33A64B-6829-4806-98FC-9B25E4ED47FC).

The `devkit` includes:

 - include files and libs for the 3ds Max USD SDK (the core MaxUsd library enabling third-party developers to extend the 3ds Max USD plugin); 
 - include files and libs for OpenUSD.  The DLLs for OpenUSD are to be found in the installed 3ds Max USD plugin. The `Pixar_USD` folder from the `devkit` contains a Python script to copy those missing DLLs; 
 - include files and binaries (libs and dlls) for UFE and USDUFE;
 - include files and libs for Python, PySide and Shiboken. The Python and PySide/Shiboken DLLs are not required to get a working 3ds Max USD component as they are provided by 3ds Max already.

The `devkit` is required to build the 3ds Max USD plugin as it requires to link to Python, PySide/Shiboken, UFE and USDUFE and those dependencies cannot be recompiled on your own for compatibility reasons with 3ds max. You can either use OpenUSD from the `devkit` or rebuild your own as explained in the next section.

> * Make sure to extract the `devkit` archive from the 3ds Max USD plugin to your development folder before trying to make use of the `devkit` .
> * Once extracted, execute the Python script `copy_missing_DLLs_found_in_the_official_installation.py` found in the `Pixar_USD` folder to recreate a completed OpenUSD prebuilt library.

#### 2. Download and Build OpenUSD 

 The `devkit` available inside the 3ds Max USD plugin installation provides a prebuild version of OpenUSD. It initially contains only the include files and libs for OpenUSD. The DLLs for OpenUSD are to be found in the installed 3ds Max USD plugin. To get a full and usable library, a Python script, to be found in the `Pixar_USD` folder from the `devkit`, must be executed to copy those missing DLLs back into the `devkit`. 

The OpenUSD library can also be rebuilt to fit with your needs. See OpenUSD's official github page for instructions on how to build USD: https://github.com/PixarAnimationStudios/OpenUSD .  It is important the recommended `OpenUSD` commit ID or tag from the table below is used: 

> **note**: the OpenUSD library included in the 3ds Max USD plugin was compiled using those options : `--python --alembic --hdf --materialx`.

|               |      ![](images/logo-horizontal-color.svg)          | USD version used in 3ds Max | USD source for MaxUsd |
|:------------: |:---------------:                  |:------------------------:|:-------------------------:|
|  CommitID/Tags | Officially supported:<br> [v21.11](https://github.com/PixarAnimationStudios/OpenUSD/tree/v21.11), [v22.11](https://github.com/PixarAnimationStudios/OpenUSD/tree/v22.11), [v23.11](https://github.com/PixarAnimationStudios/OpenUSD/tree/v23.11)| 3ds Max 2022 = v21.11<br>3ds Max 2023 = v21.11<br>3ds Max 2024 = v22.11<br>3ds Max 2025 = v23.11<br>Codename Devi = v24.05 | [v21.11-MaxUsd-Public](https://github.com/autodesk-forks/USD/tree/v21.11-MaxUsd-Public)<br>[v22.11-MaxUsd-Public](https://github.com/autodesk-forks/USD/tree/v22.11-MaxUsd-Public)<br>[v23.11-MaxUsd-Public](https://github.com/autodesk-forks/USD/tree/v23.11-MaxUsd-Public) |

The OpenUSD component has dependencies that are being reused to build the 3ds Max USD component (boost and TBB are dependencies to the 3ds Max USD) . Their source files are automatically fetched and built by the build script of OpenUSD. The table below reports on the various dependencies being used by the compiled version of OpenUSD found in the 3ds Max USD plugin.

| Dependency       | 3ds Max 2022 | 3ds Max 2024 | 3ds Max 2024 | 3ds Max 2025 |
|:----------------:|:------------:|:------------:|:------------:|:------------:|
| zlib             | 1.2.13 | 1.2.13 | 1.2.13 | 1.2.13     |
| boost            | 1.70.0       | 1.70.0       | 1.76.0       | 1.81.0       |
| TBB              | tbb2018 (update 6)| tbb2018 (update 6)| tbb2019 (update 6)| tbb2020.3       |
| HDF5             | 1.10.0 (patch 1)| 1.10.0 (patch 1)| 1.10.0 (patch 1)| 1.10.0 (patch 1) |
| OpenEXR          | 2.3.0| 2.3.0| 2.5.2| 3.1.11       |
| Alembic          | 1.7.10| 1.7.10| 1.7.10| 1.8.5       |
| MaterialX        | 1.38.0| 1.38.0| 1.38.4| 1.38.8       |
| OpenSubDiv       | 3.4.3| 3.4.3| 3.5.0| 3.5.1     |

> :warning: Make sure that you don't have an older USD locations in your ```PATH``` and ```PYTHONPATH``` environment settings. ```PATH``` and ```PYTHONPATH``` are automatically adjusted inside the project to point to the correct USD location. See ```cmake/usd.cmake```.

#### 3. Universal Front End (UFE)

The Universal Front End (UFE) is a DCC-agnostic component that allows the 3ds Max USD component to browse and edit data in multiple data models. This allows 3ds Max to edit pipeline data such as USD, using UDSUFE.  UFE is developed as a separate binary component, and therefore versioned separately from 3ds Max.

The UFE component v5.3.0 is being used.
The UFE component was originally developed by the Autodesk Maya team.
Reference documentation - https://help.autodesk.com/view/MAYADEV/2025/ENU/?guid=MAYA_API_REF_ufe_ref_index_html

To build the 3ds Max USD component, you will need to use the UFE/USDUFE headers and libraries included in the `devkit`.

#### 4. Python Modules

The project exposes some Python APIs which rely on Qt bindings that are possible through the use of PySide6 (or PySide2) and its specific Qt binding library, Shiboken.

For 3ds Max 2025 and greater, PySide6 6.5.3 is required. For 3ds Max 2022 to 2024, PySide2 5.15.1 is required. PySide2/6 are compiled in a custom way for 3ds Max, as such you can get the pre-built binaries in the `devkit`.

The 3ds Max USD Plugin also requires PyOpenGL to make use of the OpenUSD UsdView tool. The module should already present in your Python environment if you have built OpenUSD.

PyOpenGL can be downloaded using Python PIP with the following command: `pip install --user PyOpenGL==3.1.5`

> :warning: Make sure there are no PySide6/2 Python modules installed in your '*site-packages*' folders. You might run into issues otherwise, as it will create conflicts with the 3ds Max packages.

#### 5. MaterialX plugin for 3ds Max (optional)

In order to have support for MaterialX materials inside 3ds Max, you will need to hook up the MaterialX plugin for 3ds Max to the project. The MaterialX Plugin is installed alongside USD for Autodesk 3ds Max Plugin. Instructions for installation can be found here: https://help.autodesk.com/view/3DSMAX/2025/ENU/?guid=GUID-1A33A64B-6829-4806-98FC-9B25E4ED47FC

Once the 3ds Max USD plugin is installed, you will find the `MaterialX for 3ds Max` plugin located in `Contents\MaterialX_plugin`.

#### 6. Other Dependencies

| Dependency       | 3ds Max 2022 | 3ds Max 2023 | 3ds Max 2024 | 3ds Max 2025 | Link                                          |
|:----------------:|:------------:|:------------:|:------------:|:------------:|:---------------------------------------------:|
| spdlog           | 1.14.1       | 1.14.1       | 1.14.1       | 1.14.1       | https://github.com/gabime/spdlog              |
| gtest            | 1.8.1        | 1.8.1        | 1.11.0       | 1.11.0       | https://github.com/google/googletest/releases |

##### spdlog

The project is only using the headers from this dependency. You can clone the git repository and locate the include folder (the include path is at the root of the repository).

##### gtest
GoogleTest is used to compile and execute the unit tests of the component. The library is not required to run the plugin inside 3ds Max. You need to build the dependency as it is not provided through the `devkit`.

### Building
#### General Information
A Microsoft Visual Studio solution (`usd-component.sln`) for the 3ds Max USD component is provided in the `src\` folder. Make sure to fully read this section before trying to compile the component.

The component can be built for multiple 3ds Max yearly releases (referred as a *target* version). The Visual Studio solution for the component only support one *target* build at a time.

The solution creates a `build\bin\x64\<Hybrid|Release>\usd-component-<target>\` folder in which an Autodesk Application Plugin folder structure is constructed. The build folder can be directly referenced in the `ADSK_APPLICATION_PLUGINS` environment variable for 3ds Max to find and load the compiled plugin. More general information on the plugin packaging can found in the [3ds Max Developer Help](https://help.autodesk.com/view/MAXDEV/2025/ENU/?guid=packaging_plugins).

Edit the file `src\3dsmax.common.settings.props` to set the *target* version (i.e. `<VersionTarget Condition="'$(VersionTarget)'==''">2025</VersionTarget>` to target 3ds Max 2025).

Make sure the dependencies are prepared before launching the build. The next section is important to read to understand the expected structure.

#### Plugin Dependency Setup
Before starting to build the plugin, the component's dependencies location must be specified to the component's solution. A few options are possible (only use one of the options):

* [_recommended option_] dependencies specific paths can be detailed in the props file `src\DependencyPathOverrides.props`; the paths specified using the props file will override the *defaults* from the component solution.

* the component's Visual Studio solution expects a default directory structure has described below. If you are building the component for multiple _target_ version, this option might be preferable. 
  * `artifacts` folder at the root level
  * non-*target*-dependent dependencies can be placed in the `artifacts` folder:
    * `artifacts\spdlog` (the include folder for `spdlog` is placed inside this `spdlog` folder)
  * for each *target* version, a *target* folder (i.e. : `artifacts\2025` is needed for the remaining dependencies:
	  * `artifacts\<target>\<target>_3dsmax-component-materialX`
	  * `artifacts\<target>\gtest`
	  * `artifacts\<target>\maxsdk`
	  * `artifacts\<target>\Pixar_USD`
	  * `artifacts\<target>\PyOpenGL`
	  * `artifacts\<target>\PySide6\PySide6` (might be `PySide2` depending on the *target*)
	  * `artifacts\<target>\PySide6\shiboken6`
	  * `artifacts\<target>\PySide6\shiboken6_generator`
	  * `artifacts\<target>\Python`
	  * `artifacts\<target>\Qt`
	  * `artifacts\<target>\ufe`
	  * `artifacts\<target>\UsdUfe`

* a Python build script (`build-scripts\build-solution.py`) is provided to launch the build from a command prompt. If not already described in `src\DependencyPathOverrides.props` or not using the default directory structure, the dependency paths can specified directly at the command prompt. This option might be easier to use if you are scripting your build procedure.

The build script can be used by following these usage rules:

	usage: build-solution.py [-h] [-b BUILD] [-v VERSION] [-w] [-r] [-d] [-p] [--maxsdk MAXSDK] [--qtinstall QTINSTALL]
                         [--materialx MATERIALX] [--googletest GOOGLETEST] [--pyopengl PYOPENGL]
                         [--maxusddevkit MAXUSDDEVKIT] [--spdlog SPDLOG] [--python PYTHON] [--pyside PYSIDE]
                         [--shiboken SHIBOKEN] [--ufeinc UFEINC] [--ufelib UFELIB] [--usdufe USDUFE]
                         [--openusd OPENUSD] [--tbb TBB] [--boostinc BOOSTINC] [--boostlib BOOSTLIB]
                         [{release,hybrid}] {2022,2023,2024,2025,2026}

	positional arguments:
      {release,hybrid}      The build configuration type.
      {2022,2023,2024,2025,2026}
                            The 3ds Max version to target.
    
    optional arguments:
      -h, --help            show this help message and exit
      -b BUILD, --build BUILD
                            The build number coming from the pipeline.
      -v VERSION, --version VERSION
                            The 3ds Max USD component version being built.
      -w, --warnaserror     Enable the compiler to treat all warnings as errors.
      -r, --rebuild         Rebuild the project.
      -d, --distrib         Prepare for redistribution. Write component version in source headers.
      -p, --package         Prepare the package folder after build.
      --maxsdk MAXSDK       The path location for the 'MaxSDK' folder.
      --qtinstall QTINSTALL
                            The Qt reference version from QtVsTools (aka 'Qt Installation').
      --materialx MATERIALX
                            The path location for the 3ds Max MaterialX material plugin folder.
      --googletest GOOGLETEST
                            The path location for the 'gtest' folder.
      --pyopengl PYOPENGL   The path location for the 'OpenGL' Python module (PyOpenGL).
      --maxusddevkit MAXUSDDEVKIT
                            The path location for the 3ds Max USD 'devkit'.
      --spdlog SPDLOG       The path location for 'spdlog' include folder. If not provided, using the path from the
                            'devkit' if the 'maxusddevkit' option is provided.
      --python PYTHON       The path location for the 'Python' folder. If not provided, using the path from the 'devkit'
                            if the 'maxusddevkit' option is provided.
      --pyside PYSIDE       The path location for the 'PySide6' Python module. If not provided, using the path from the
                            'devkit' if the 'maxusddevkit' option is provided.
      --shiboken SHIBOKEN   The path location for the 'shiboken6' Python module. If not provided, using the path from the
                            'devkit' if the 'maxusddevkit' option is provided.
      --ufeinc UFEINC       The path location for the 'Ufe' include folder. If not provided, using the path from the
                            'devkit' if the 'maxusddevkit' option is provided.
      --ufelib UFELIB       The path location for the 'Ufe' lib folder. If not provided, using the path from the 'devkit'
                            if the 'maxusddevkit' option is provided.
      --usdufe USDUFE       The path location for the 'UsdUfe' folder. If not provided, using the path from the 'devkit'
                            if the 'maxusddevkit' option is provided.
      --openusd OPENUSD     The path location for the 'OpenUSD' folder. If not provided, using the path from the 'devkit'
                            if the 'maxusddevkit' option is provided.
      --tbb TBB             The path location for the 'TBB' folder. If not provided, using the path from the 'OpenUSD' if
                            the 'openusd' or 'maxusddevkit' option is provided.
      --boostinc BOOSTINC   The path location for the 'Boost' include folder. If not provided, using the path from the
                            'OpenUSD' if the 'openusd' or 'maxusddevkit' option is provided.
      --boostlib BOOSTLIB   The path location for the 'Boost' lib folder. If not provided, using the path from the
                            'OpenUSD' if the 'openusd' or 'maxusddevkit' option is provided.

For example, if you have:

 - cloned the `3dsmax-usd` repository to `c:\dev\3dsmax-usd`
 - you are targeting to build the plugin for 3ds Max 2025
 - you have installed the Autodesk USD for 3ds Max 2025 application plugin, by default, to `C:\ProgramData\Autodesk\ApplicationPlugins\USD for 3ds Max 2025`
 - extracted its `devkit` to `c:\dev\3dsmax-usd-devkit-2025` (the `devkit` archive would be found in `C:\ProgramData\Autodesk\ApplicationPlugins\USD for 3ds Max 2025\Contents`)
 - execute the Python script `copy_missing_DLLs_found_in_the_official_installation.py` found in the `c:\dev\3dsmax-usd-devkit-2025\Pixar_USD` folder to recreate a completed OpenUSD prebuilt library
 - installed the 3ds Max 2025 SDK, by default, to `C:\Program Files\Autodesk\3ds Max 2025 SDK`)
 - Qt v6.5.3 is installed in `C:\Qt\6.5.3\msvc2019_64`
 - PyOpenGL is installed in `C:\Users\myusername\AppData\Roaming\Python\Python311\site-packages`
 - GoogleTest is installed in `c:\dev\googletest-distribution`

You will have a command-line similar to the one below. Using the `devkit` reduces the amount of steps required to get to a state where the component can be built.

	c:\dev\3dsmax-usd> \
	python build-scripts\build-solution.py \
	--maxusddevkit c:\dev\3dsmax-usd-devkit-2025 \
	--googletest c:\dev\googletest-distribution \
	--qtinstall c:\Qt\6.5.3\msvc2019_64 \
	--maxsdk "c:\Program Files\Autodesk\3ds Max 2025 SDK\maxsdk" \
	--materialx "c:\ProgramData\Autodesk\ApplicationPlugins\USD for 3ds Max 2025\Contents\MaterialX_plugin" \
	--pyopengl C:\Users\myusername\AppData\Roaming\Python\Python311\site-packages -p release 2025

