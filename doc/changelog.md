## Changelog

### v0.10.0

#### What's New:
- Added support for displaying USD Curves in the viewport. Note that the display only supports linear interpolation at this time.
- Added a menu in the USD Collection Widget to remove all prims from the Include/Exclude lists at once.

#### Fixes:
- Fixed an issue where Collection rollouts could modify the Command Panel layout.
- Fixed error where prims in the USD Collection Widget could get deleted unexpectedly.
- Fixed an issue where Collections for some Prims were not being populated in the Rollouts.
- Fixed an issue where Light Linking Rollout wouldn't load in older versions of 3ds Max.
- Fixed label in the Undo Stack of the Mute USD Layer action.
- Fixed an issue where the Title for the modal window for Picking Prims for the Light Linking should reflect where it's being added from.
- Fixed a problem where the USD Collection widget could sometimes become unresponsive when the rollout had been resized.
- Fixed the USD Exporter not adding the extent property to USDBasisCurves when exporting a shape from 3ds Max.
- Fixed error where locking a layer on a USD Stage would clear the current prim selection.
- Fixed a crash in the USD Stage Object when switching between kinds and prim object/sub-object mode.
- Fixed undo getting lost when a USD Layer is muted.
- Fixed the bulk remove of sublayers in the USD Layer Editor not always respecting the entire selection.
- Fixed USD Light gizmos displaying in the viewport even when the prim is hidden.
- Fixed an issue where the Viewport Selection Rollout was missing from the Rollouts while in Prim Sub-object Mode.
- Updated the USDStageObject to accept loading a USD stage that has no prims.
- Fixed error when redoing an action after doing an undo after prim creation in the USD Stage.

### v0.9.8

#### What's New:
- Updated the USDStageObject to preserve the current EditTarget when reloading a scene.
- Added option in USD Stage Object to control the gizmo sized of light shapes.
- Added prompt to Save or Discard unsaved edits in USD Layers when reloading a stage that has edits that are not yet saved.
- Updated menus for creating a USD Stage from file to automatically select the USDStageObject upon creation.
- Updated the USD Layer Editor to set layers to System Lock when the layer is a read-only file on disk.
- Updated lights gizmos from a USD stage to appear yellow to match Max behaviors. Selected lights are still the color of the USD Stage Object selection color.
- Added a new USD Prim Pick mode that can be initiated from the new UsdSharedComponents extension. To load the extension use `maxExtension = Python.Import("UsdSharedComponents.maxExtension");` to call the pick mode, use `maxExtension.PickPrim(stage)` where stage is a python object storing the stage. This function acts similarly to the MAXScript `PickObject()` method but is for returning UFE paths that allow you to process the picked prims for custom purposes.
- Update Max USD to default to the Root Layer as the Edit Target when creating a new stage object. Previously we targeted the session layer by default, but now that the Layer Editor is available we moved it to the Root Layer.
- Added "Class Prims" entry into the USD Explorer Display menu to Show/Hide USD prims in the USD Explorer.

#### Fixes:
- Fixed a crash in the USD Stage Object when switching between kinds and prim object/sub-object mode.
- Fixed the display of prim hierarchy in the USD Explorer when adding a prim to a class prim.
- Fixed defect where reordering layers in the USD Layer Editor could remove the layers entirely.
- Fixed incorrect warning of unsaved edits in a stage when exporting a scene with a stage that generated session data for automatic pointInstance draw modes.
- Properly expose the 3ds Max USD python method `maxUsd.JobContextRegistry.GetJobContextInfo()`.
- Fixed the USD Layer Editor to hide the Remove Layer menu for the root layer and session layer.
- Fixed warning icon in the USD Layer Editor missing from layers that cannot be resolved.
- Fixed USD Layer Editor DPI scaling issues.

### v0.9.7

#### What's New:
- Added option in USD Stage Object to control the gizmo size of light shapes.
- Updated light gizmos from a USD stage to appear yellow to match 3dsMax behaviors. Selected lights are still the color of the USD Stage Object selection color.
- Updated the Display rollout in a USD Stage Object to remain accessible when in Prim Sub-Object mode.
- Updated OpenEXR to OpenEXR 3.3.1 in Max USD.
- Updated the Display rollout in a USD Stage Object to remain accessible when in Prim Sub-Object mode.
- Added an option to display class prims in the USD Explorer.
- Added a confirmation dialog when reloading USD stages.

#### Fixes:
- Fixed error loading some USD Layers in the Layer Editor.
- Fixed bug causing reconsolidation on light gizmo selection.

### v0.9.6

#### What's New:
- Exposed the USD Layer Editor commands to Python.
- Added Edit Restrictions to Max USD to block edits when there is a stronger opinion for that property.
- Added ability to load a sublayer to a layer in the USD Layer Editor.
- Added a bulk save dialog for the USD Layer Editor.
- Updated the MaterialX Exporter in Max USD to use nodegraphs.
- Added support to export a materialX reference using OpenPBR materials in the MaterialX shader writer for USD.

#### Fixes:
- MaterialX Scripted material fails more gracefully when the import fails.

### v0.9.5

#### What's New:
- Updated the USD Layer Editor to correctly respect 3ds Max color themes.
- Added ability to create a new stage from file from the USD Layer Editor.
- Added options for saving dirty layers in a USD Stage when saving a Max scene. Note that in the current iteration, this will create a pop-up even during Autosave to determine what to do with dirty layers. We intend to address this in a coming update.
- The bulk save function in the USD Layer Editor displays the number of layers needing saved.
- Added a right-click menu to print a layer in the USD Layer Editor to the MAXScript Listener. If the layer is more than 400 lines or 50K characters, the function will ask to confirm the print before printing since very large datasets can take a long time to print.
- The USD Layer Editor can now reload a selected layer.
- The Mute and Lock states of a USD Layer is now preserved between Max sessions.
- Users can now launch the USD Layer Editor from the USD Explorer as well as the `Tools > USD > USD Layer Editor...` menu.

### v0.9.4

#### What's New:
- Added support for displaying USD Light shapes in the viewport. Note they do not cast lights into the viewport but can be selected and edited. The results will render in renderers with USD support.
- Update some labels and tooltips around the Draw Mode functions in the USD Stage Object.
- Updated the MaterialX Plugin to use MaterialX version 1.38.8 for 3ds Max 2025.

#### Fixes:
- Fixed some of the shipped tools such as USDView not working because of Powershell security policy settings.
- Fixed an issue where Selecting the USD Stage from a recently edited prim wouldn't always work.
- Animated USD attributes now refresh when scrubbing the timeline.
- Prevent situation where deactivating parent and child leave selection in state that can cause crash.
- Fixed incorrect progress reporting in the USD Importer.
- Fixed an issue where Display Purpose may not sync correctly when changing the file reference in an existing USD Stage or when changing display purposes.

### v0.9.3

#### What's New:
- Updated the USD Exporter to better convert Max Shapes into USD instead of always exporting to linear interpolations.
- Added USD Layer Editor to target layers for USD edits.
- Updated the MaterialX Material to save the current material by name instead of index. This guarantees that sub-materials in a USD remain intact in Max even if the MaterialX document changes the ordering of materials.
- Exposed more of the MaterialX Exporter options to MAXScript.
- Updated the MaterialX plugin to support importing MaterialX documents using OpenPBR shaders.
- Introducing the concept of a devkit archive delivered inside the 3dsMax USD plugin installation. The devkit includes all the 3ds Max USD SDK files (includes and libs), the samples and the minimal dependencies (includes and libs) required to compile the samples (or any third-party add-on plugin for the 3ds Max USD plugin). Additionally, the devkit contains the other dependencies required to compile the open-sourced 3ds Max USD plugin (still in preparation). The previous 3ds Max USD SDK archive is not produced anymore. The devkit is its replacement.

#### Fixes:
- Fixed an issue where the Material Export toggle in the USD Exporter was incorrectly reporting as enabled, when in fact it was disabled.
- Deactivating parent and child leave selection in state that can cause crash.

### v0.9.2

#### What's New:
- Made some USD rollouts more human readable instead of always coming directly from USD Schemas.

#### Fixes:
- Fixed and issue where USD Stage roll-ups would not retain their user set state when switching to sub-object mode.
- The MaterialX Exporter now writes out "sRGB" colorspace instead of "gamma22".

### v0.9.1

#### What's New:
- Compatibility release for 3ds Max Beta D2634-67.16 release.

### v0.9.0

#### What's New:
- Bump component version to 0.9 - new public release.

#### Fixes:
- Changed MaterialX plugin utility to export using colorspace "sRGB" by default instead of "gamma22".
- Fixed an issue where setting Textures for Draw Modes in the Prim Attributes could cause 3ds Max to become unresponsive.
- Cleaned up USD Attribute Tooltips.
- Fix expected behavior on PrimReader plugin not being loaded when 'providesTranslator' type is an ancestor type.
- Fixed spinner for USD Prims to allow negative values.
- Fixed options for loading a stage not persisting between multiple stage-loading actions.
- Fixed unnecessary memory usage from textures that should not be loaded from deactivated prims in a USD Stage Object.
- Update some labels and tooltips around the Draw Mode functions in the USD Stage Object.
