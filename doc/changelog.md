## Changelog


### v0.13.0

#### What's New:
- New public release - version 0.13.0

#### Fixes:
- After updating to OpenUSD v0.25.08, 3ds Max is crashing when mouse hovering a USD stage.
- Fix edit box cropping letters below line in Asset Resolver preference dialog

### v0.12.5

#### What's New:
- 3ds Max Beta version now uses OpenUSD v0.25.08. No changes to 3ds Max 2026 and earlier.
- Added a new option in the animation section of the USD Exporter to include animation bones that are not being referenced by a skin modifier.
- Added an option in the animation section of the USD Exporter to preserve or not the original 3ds Max bones when exporting Usd Animations.
- Added a new option in the animation section of the USD Exporter to simplify bone paths when exporting Usd Animations.
- Added UX for managing search paths with the USD Asset Resolver.

#### Fixes:
- Fixed the Asset Resolver resolving the when the ADSK_AR_MAPPING_FILE environment variable was set.
- Fixed defect where the Duplicate as USD Data menu in the quad menus could not be customized.
- Fixed the USD Menus not having the top handle bar.
- Fixed EXCEPTION_ACCESS_VIOLATION error when trying to export with contentSource set to #nodeAndMaterialList and no nodes where passed in the function.
- Fixed a defect where a USD stage would not be found after renaming paths and opening a stage file in certain orders.
- Fixed defect where USD edits saved into a Max 2023 scene would not open correctly in 3ds Max 2026.
- Fixed an issue where the Skin options in the USD Exporter could be available when they ought to be disabled in the UI.
- Fixed an issue where exporting Materials without Geometry, where the nodegraph used Multi-materials would not export the materials correctly.
- Fixed an unexpected error after saving a 3ds Max file with anonymous stage
- Fixed an issue with .AvailableChasers() returning unexpected values.

### v0.12.4

#### What's New:
- Optimized the elementSize of joinIndices and jointWeights in USD Skel to remove unnecessary data.
- Added support for bulk deleting of prims in the USD Explorer.
- Added new USD menu to the main toolbar.
- Automatically opens the USD Explorer when creating a new USD Stage that has an anonymous root layer.
- Added a new option in the animation section of the USD Exporter to export Animation Curves. When enabled, camera properties will exported as animation curves in USD 24.11+.
- Updated the USD Exporter with a new Material option to handle Shell Materials. The options are to Use Baked, Us Original and Use Baked and Original. When using baked and original, sets the appropriate material purposes to "full" and "preview" and binds both materials to prims.

#### Fixes:
- Fixed an issue with MaterialX referencing on Import.

### v0.12.3

#### What's New:
- Updated the way anonymous layers are named. Instead of using the layer identifier, anonymous layers are now named with the convention of "anonymousLayerX", where X is the nth anonymous layer at the current level.
- Updated the cloning of USD prims via SHIFT+DRAG to preserve the original prim's reference instead of making a chain of references if there is a single reference entry on the dragged prim. This improves the cleanliness of the USD data.
- Automatically opens the USD Explorer when creating a new USD Stage that has an anonymous root layer.
- Added support to remember usd layer edit targets even for anonymous layers.
- Added Project-based tokens to the USD Asset Resolver. This will enable tokens that match those folders made by a standard Max project settings.
- Added new option in USD Exporter to export materials without requiring geometry in the export.
- Added a new method to the USDStageObject called SetStageFromCache(ID) that allows you to set change the stage used in a USDStageObject from a stage in the USD cache by passing the stage's cacheid.
- Added the ability to rename USD Prims in the USD Explorer and in the command panel.
- Added a Shader Reader for referenced MaterialX documents. When importing USD that has a MaterialX reference, the MaterialX will now be imported.
- Added new Transform Format to the USD Exporter. The options are "Single Matrix" (one transform operator) or "Separate Translate, Rotate and Sale" (one operator for each). The new separation creates data that can remain consistent between 3ds Max and other DCCs such as Maya.

#### Fixes:
- Fixed the USD Layer Editor failing to show a layer stack below any layers brought in using Stage Variable Expressions.
- Improved poor performance when duplicating many objects into a USD Stage.
- Updated the Stage Reload button in the command panel of a USDStageObject to only reload file-backed layers allowing anonymous root layers and anonymous layers belonging to all anonymous root layers to remain when reloading a stage.

### v0.12.2

#### What's New:
- Added first iteration of being able to copy prims in a USD Stage using Shift+Drag in the viewport. At this time, the feature is limited to making an Internal Reference to the "copied" prim, meaning it is prone to break if the source prim moves in the USD hierarchy.
- Added a menu to set/unset a prim in a USD Stage as the stage's default prim. The default prim is needed to properly reference a USD scene into other USD files without having to specify a prim path.
- Added new option in the viewport quad menus to duplicate selected Max objects into a new stage instead of requiring a stage from file or existing stage node.
- Added default values for new stages to set the up-axis, units and FPS based on the current Max scene settings.
- Added support for saving anonymous layer lock and mute states.
- Added the ability to reparent USD prims to change hierarchies in the USD Explorer.

#### Fixes:
- Fixed some UI issues in the USD Explorer showing the wrong selection information after deleting a stage with an anonymous root layer.

### v0.12.1

#### What's New:
- USDStageObject now creates an anonymous root layer by default.
- Added support to create and save anonymous layers in the USD Layer Editor.
- Added support to save a anonymous layers into a 3ds Max scene without saving any files to disk.
- Added the ability to reparent USD prims to change hierarchies in the USD Explorer.

### v0.12.0

#### What's New:
- New public release - version 0.12.0

### v0.11.4

#### What's New:
- Compatibility release for 3ds Max Beta N1548-69.11 release.

### v0.11.3

#### Fixes:
- Fixed an issue where 3ds Max could crash during USD Export of animated content with Splines present.
- Fixed missing root in the USD Explorer after some search operations.
- Fixed crash when toggling between prim subobject mode and object mode after deactivating prims in the USD Explorer.

### v0.11.2

#### What's New:
- Added new right-click quad menus for Duplicate as USD as well as options for the duplication.

### v0.11.1

#### What's New:
- Exposed functions to the new Export To USD api to send Max data directly to a stage object.
  
### v0.11.0

#### Fixes:
- Fixed incorrect transform of subsplines when exporting to USD shapes.
- Fixed crashes related to SHIFT+Clicking prims in the USD Explorer.
- Fixed freezing when deactivating parent and child USD prims then hitting undo.
- Fixed a crash when adding a Prim from the USD Explorer with an active search filter.

### v0.10.6

#### Fixes:
- Fixed an issue where Undoing 'Remove All' function from the Collection menu would not restore all list items that were removed.
- Fixed some issues with the refresh function of the USD Geometry Object not always working.
- Prevent crash when loading a 3ds Max file containing a USD stage object with an invalid stage file (missing/renamed file).
- Improved the UI of the USD Collection Widget type-ahead to be more readable.
- Fixed an issue where the USD Explorer hierarchy could collapse at the root level after some undo operations.
- Fixed an issue where changing the Root Layer while in Prim Sub-object mode would prevent the expected rollouts from being displayed.
- Fixed defect related to deactivating then reactivating a prim in USD.
- Fixed an unexpected doubling of entries in the Undo Stack when a USD Camera is generated from a USD Stage Object.
- Fixed the USD Layer Editor pin setting not being respected in some cases of add/removing USD stages in the scene.

### v0.10.5

#### What's New:
- Updated the label for Push to 3ds Max in the USD Explorer to Promote to 3ds Max.
- Updated the Promote to 3ds Max function to apply the XForm Controller to a Transform List Controller with another PRS Controller so that USD Geom Objects can be moved by the user.
- Added MAXScript exposure to the Stage and Prim Path properties of a USD Geometry Object.
- Updated and cleaned up the USD Geom Object UI in the modify tab.
- Added support for the OpenPBR Material in the USD Exporter.
- Added a help link in the USD Collection widget.
- Added new optional boolean parameter hideClassPrims to maxUsd.PickItems(). This option is true by default. When turned off, the USD Picker will not filter out class prims.
- Refreshed the USD Stage UI with some improved organization and addition of a button to launch the USD Layer Editor.

#### Fixes:
- Fixed a crash when undoing actions related to the new USD Geometry Object.
- Fixed the USD Selection getting lost when using the Promote to 3ds Max function.
- Fixed an issue where Collection rollouts could modify the Command Panel layout.
- Fixed error messages related to the USD Collection widget to appear red in the MAXScript Listener.
- Adding and removing items to collections is now properly respecting edit restrictions and prints information when some attempt to edit is not possible.
- Fixed an issue where Collections for some Prims were not being populated in the Rollouts.
- Fixed an issue where Light Linking Rollout wouldn't load in older versions of 3ds Max.
- Fixed a problem where the USD Collection widget could sometimes become unresponsive when the rollout had been resized.

### v0.10.4

#### What's New:
- Added redo support in the USD Geom Object purpose radio options.
- Added a refresh button to update the current mesh data on a USD Geom Object that isn't set to receive live updates.
- Added a warning to the USD Geom Object when there are settings that have possible conflicts with other USD Geom Objects derived from the same prim.
- Added new method to get/set selected layers in the USD Layer Editor. Python example:
```
python import UsdLayerEditor
layerIdList = UsdLayerEditor.getSelectedLayers()
UsdLayerEditor.setSelectLayers([layerIdList[0]])
```

#### Fixes:
- Fixed a defect that caused an error when passing a lambda function to usdUfe.registerUICallback().
- Fixed the Auto-Expand function not working in the USD Explorer.

### v0.10.3

#### What's New:
- Added new option to include/exclude prims by visibility in the Push to Max workflow in USD.
- Added option to choose what purposes are used with the Push to Max workflow from USD.
- Added lazy loading to the USD Explorer to increase performance of loading a stage into the explorer.
- Added option to serialize USD changes into the Max file for layer edits that haven't been saved to disk.

### v0.10.2

#### What's New:
- Added menu to the USD Collection Widget to copy the path to the collection.
- Added new menu in USD Explorer to Push a prim mesh to a geometry object in Max.
- Added undo support to the USD Collection Widget used in Light Linking.
- Added the ability to select prims in a scene from the USD collection widget (like light linking).

#### Fixes:
- Fixed an issue where some rollups for Prim Sub-object mode were not being loaded during selection.
- Fixed the Include All not getting toggled off when adding prims to an include list for a USD Collection.
- Fixed an error when selecting the Root prim when appending to Include/Exclude lists using the Picker.
- Fixed USD Export of camera lens and filmback properties to be measured in tenths of a scene unit rather than in millimeter units to comply to UsdGeomCamera units.
- Fixed problem where deactivating both parent and child prims could lead to a state where not all the selected prims were deactivated.
- Cleaned up some USD Attribute tooltips.
- Fixed an issue where deactivating then re-activating prims could change the USD Explorer to show unexpected prototype prims.

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
