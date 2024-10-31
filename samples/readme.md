# Samples

The sample projects illustrate how to implement the available APIs. They are constructed to be easily added individually as application plugins to 3ds Max. Remember however that those extensions are not 3ds Max plugins but are plugins to the 3ds Max USD plugin, and are registered in a different way.

The samples are provided as a starting point to help you understand the basic elements to put in place in C++ as well as in Python. They are basic examples of possible extensions a user might want to put in place and are not meant to be full implementation of the feature they are providing.

The Python version of the samples are functional directly from the provided devkit package. You simply need to add the path of the samples root folder or the specific sample you wish to use to the `ADSK_APPLICATION_PLUGINS` environment variable.

The C++ version of the plugins need to be compiled using the same requirements as for 3ds Max plugins. As of today, we are providing all component dependencies along with the devkit package. You will need to edit the `ADSK_APPLICATION_PLUGINS` environment variable to add the `build\samples\bin\x64\<Configuration>\` that got generated at compile time. Also, the sample plugins are configured to be running their Python version by default. Do not forget to edit their `RegisterPlugin.ms` MAXScript post-launch script to enable the C++ section and comment out the Python section of the script.

- **glTFMaterialWriterPlugin** - an example of how to implement a `ShaderWriter` that exports glTFMaterial to UsdPreviewSurface.  This is a minimal, non-exhaustive example.
- **SpherePrimWriterPlugin** - an example of how to implement a `PrimWriter` that exports 3ds Max spheres to USD native spheres.  
- **SpherePrimReaderPlugin** - an example of how to implement a `PrimReader` that imports USD native spheres to 3ds Max spheres.  
- **UserDataExportChaserPlugin** - an example of how to implement an `ExportChaser` to expose user properties and custom attributes to a USD custom data node.
- **UserDataImportChaserPlugin** - an example of how to implement an `ImportChaser` to import custom data to a 3ds Max user properties and custom attributes.

> The **Qt** user interface code of the *UserDataExportChaser C++ Sample* requires an installation of **Qt** as described in the official 3dsMax SDK documentation. To skip the Qt UI part, removing the Qt code part from the sample is possible as
well, the rest of the sample will still work regardless (analogous to the UserDataImportChaserSample, that does not have any Qt dependency).

## Requirements
The requirements for using this 3ds Max USD SDK are the same as the [3ds Max SDK](https://help.autodesk.com/view/MAXDEV/2023/ENU/?guid=sdk_requirements). 

The Qt library as described in the official [3ds Max SDK documentation](https://help.autodesk.com/view/MAXDEV/2025/ENU/?guid=sdk_requirements).  
We also recommend to install the _QtVSTools for Visual Studio_ to automatically use the QtMSBuild tools provided by this extension. It may be required to choose the appropriate Qt installation in the project settings - based on your targeted version of 3dsMax.

> The 3ds Max USD component devkit includes pre-builds of the component's dependencies. The devkit is found within an installed 3ds Max USD component.

# Plugin development
## Creating a plugin project

Plugins need to minimally link against the `maxsdk`, `openusd` and the `maxUsd` (from the 3ds Max USD component) libraries
**core.lib;maxutil.lib;maxUsd.lib and 3dsmax_\<openusdlibs\>.lib**

> The  `openusd` libraries are included in the devkit.

> The requirements for using this SDK are the same as the  [3ds Max SDK](https://help.autodesk.com/view/MAXDEV/2023/ENU/?guid=sdk_requirements).

## Registering a plugin

Plugins are registered by providing a path or paths to JSON files that describe the location, structure and contents of the plugin. The standard name for these files is `plugInfo.json`.

See the [USD Registry Reference](https://graphics.pixar.com/usd/dev/api/class_plug_registry.html#details) for more information.

Every provided samples are following those principles.

The Info sub-section in the plugInfo schema for all 3ds Max USD plugins contains those nested properties:

- `MaxUsd` : Mandatory property identifying the 3ds Max Info block.
  - `PrimWriter` : Optional property stating the current plugin contains PrimWriters.
   - `PrimReader` : Optional property stating the current plugin contains PrimReader
      - `providesTranslator` : Required property listing the Usd Prim Types the PrimReader imports; the strings making up the array are the Usd Prim Type name
  - `ShadingModePlugin` : Optional property stating the current plugin contains a ShadingMode definition.
  - `ShaderWriter` : Optional property stating the current plugin contains ShaderWriters.
    - `providesTranslator` : Required property for ShaderWriters, listing the translated 3ds Max material; the strings making up the array are the material non-localized names.
  - `ShaderReader` : Optional property stating the current plugin contains ShaderReader
      - `providesTranslator` : Required property listing the imported Usd Shader Ids; the strings making up the array are the token value of Usd Shader Id
  - `ExportChaser`: Optional property stating the current plugin contains an ExportChaser.
  - `ImportChaser` : Optional property stating the current plugin contains ImportChaser
  - `JobContextPlugin` : Optional property stating the current plugin contains a JobContext.

Here is a possible sample `plugInfo.json` file declaring a C++ plugin containing the implementation of:

- a PrimWriter handling the translation of 3ds Max Nodes,
- a new material target to (material conversion type) or a shader mode exporter (both part of the ShadingModeRegistry),
- a ShaderWriter handling the translation of the 3ds Max "Physical Material" material type,
- an ExportChaser fine-tuning the export stage.
- a JobContext (plug-in configuration) wrapping a function to set options to use at import/export

```json
{
   "Plugins":[
      {
         "Info":{
            "MaxUsd":{
               "PrimWriter": {},
               "ShadingModePlugin": {},
               "ShaderWriter":{
                  "providesTranslator":[
                    "Physical Material"
                  ]
               },
               "ExportChaser": {}
               "JobContextPlugin": {}
            }
         },
         "Name":"sampleMaxUsdPlugin",
         "Type":"library",
         "LibraryPath":"SampleMaxUSDPlugin.dll"
      }
   ]
}
```

In case of a Python plugin, the plugin script location must part of the Python path (this can be achieved in a startup script with the Python command `sys.path.insert`).

For the USD PlugRegistry to find the `plugInfo.json` file, the file directory must be registered.

```python
from pxr import Plug
Plug.Registry.RegisterPlugins(<usdPluginsPath>)
```

Again, this can be achieved in a 3ds Max startup script for the plugin, or you can use the environment variable **PXR_PLUGINPATH_NAME** to define the USD plugin path (the path where the `plugInfo.json` file is located).

Regardless of the type of plugin, C++ or Python, to register your plugin you'll need to add a registration script to your 3ds Max plugin in the "post-start-up scripts parts", these USD plugins need to be loaded only when Max is ready to be interacted with and NOT be loaded by the 3ds Max plugin itself prior to that. You can find examples of such scripts in the SDK sample projects.

When creating a C++ plugin (.dll), it should be a separate project from your actual 3ds Max plugin. USD plugin DLLs should be loaded via the USD plugin API, USD does not expect the DLL to be already loaded when the plugin is loaded - and if it is, that can create issues.

For a C++ plugin it is also very important to set the project option "Remove unreferenced code and data" to NO. Not doing so could cause the Macro to be optimized out and the Writer to never be properly registered.
