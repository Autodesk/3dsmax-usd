![3ds Max USD](doc/images/header.png)
# 3ds Max USD Plugin
**3dsmax-usd** is a feature-rich plugin for 3ds Max that provides support for [OpenUSD](http://openusd.org/) as part of [AOUSD](https://aousd.org/).  While the plugin provides traditional methods for import and export, the most important and exciting capability it provides is to load USD stages directly in the viewport for interactive editing without import/export.

![3ds Max USD](doc/images/3dsmax-usd.png)

The USD for 3ds Max extension lets you create, edit, work in, work with and collaborate on USD data, while enabling data to move between products (ie. Maya and 3ds Max). By enabling USD data to flow in and out of 3ds Max, you can take advantage of the following key benefits of USD: supporting DCC-agnostic pipelines/workflows and enabling non-linear collaboration. While not necessarily a replacement for the core 3ds Max referencing workflows (Object XRef and Scene XRef), the USD features can be used as a modern cross-DCC referencing pipeline that can enhance, and in many use-cases, replace existing referencing setups.

The plugin comes with a complete API to allow extending the default import and export process and also to manipulate USD data directly via C++ and Python.


## Features
- Import and Export ASCII And Binary USD formats
- Open a USD Stage directly for editing
- USD Explorer
- USD Cameras
- USD Lights
- 3ds Max Controller Support
- MaterialX Material

# Additional Information
- [Building](#building)
- [Coding Standards](doc/CodingGuidelines.md)
- [Contributing](#contributions)
- [Developer Documentation](#developer-documentation)
- [Security](#security)
- [Supported Versions](#versions)




## Building
Everything needed to build MaxUSD is provided in the form of source and a Devkit that needs to be installed.  Unit tests are provided for all projects and can be optionally built and executed using google tests. Full details on how to build and test 3dsmax-usd can be found in the [build documentation](doc/build.md).

## Contributions
We welcome your contributions to enhance and extend the tool set.  Please visit the [contributions](doc/CONTRIBUTING.md) page for further information.

## Developer Documentation
MaxUSD comes with extensive documentation and [samples](samples/readme.md).  For more information on Maxscript exposure please reference the [online documentation](https://help.autodesk.com/view/MAXDEV/2025/ENU/?guid=MAXScript_USD_overview_html) or ask questions in the [discussion pages](https://github.com/Autodesk/3dsmax-usd/discussions).  OpenUSD has a very active [forum](https://forum.aousd.org/) for specific USD questions or discussions.

## Security
We take security serious at Autodesk and the same goes for our open source contributions.  Our guidelines are documented [here](SECURITY.md)

## Versions
3ds Max USD actively supports the following versions of 3ds Max.
- 2022
- 2023
- 2024
- 2025
