//
// Copyright 2023 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#pragma once

#include "USDSceneBuilderOptions.h"

#include <MaxUsd/Chaser/ExportChaserRegistry.h>
#include <MaxUsd/DLLEntry.h>
#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/Translators/AnimExportTask.h>
#include <MaxUsd/Translators/PrimWriterRegistry.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <GetCOREInterface.h>
#include <lslights.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief USD Scene Builder.
 * \remarks This current implementation is a work-in-progress that will evolve as additional conversion operations
 * between USD and 3ds Max are supported. Performance of the import process is a design concern, and
 * while CRTP-type solutions are not (currently) implemented, future work should attempt to
 * improve/maintain run-time performance while maintaining a high level of flexibility.
 * \remarks This current implementation moves some of the import logic away from the USDSceneController where it was
 * previously located. In the process, the import still owns some of the UI/UX import process such
 * as the handling of 3ds Max's progress bar. Future work should abstract away this behavior, and
 * expose more control to the caller (e.g. through callbacks, or notifications about the current
 * state of the import process, etc.).
 */
class USDSceneBuilder
{
public:
    /**
     * \brief Constructor.
     */
    USDSceneBuilder();

    /**
     * \brief Destructor.
     */
    virtual ~USDSceneBuilder() = default;

    /**
     * \brief Build a USD Stage from the given build options.
     * \param buildOptions Build configuration options to use during the translation process.
     * \param cancelled Flag that will be set to true if cancelled by user.
     * \param filename The filename of the USD file that is being built.
     * \param editedLayers Identifiers of layers edited during the export. All these layers will be saved to disk at the end of the export process.
     * \param isUSDZ flag that will be set to true if exported file will end up packaged in a usdz archive.
     * \return The USD Stage built from the given options.
     */
    pxr::UsdStageRefPtr Build(
        const USDSceneBuilderOptions&               buildOptions,
        bool&                                       cancelled,
        const fs::path&                             filename,
        std::map<std::string, pxr::SdfLayerRefPtr>& editedLayers,
        bool                                        isUSDZ);

protected:
    /**
     * \brief Context for each translation operation to be performed as part of the USD Stage building process.
     */
    struct TranslationContext
    {
        /// Reference to the 3ds Max Node to translate:
        INode* node { nullptr };

        /// Reference to the USD Stage into which to perform the translation:
        const pxr::UsdStageRefPtr& stage;
        /// Path where to create the new prim(s):
        const pxr::SdfPath parentPrimPath;

        /// Name to give the new prim
        const std::string primName;

        /// USD time configuration for the translation operation:
        const MaxUsd::TimeConfig timeConfig;

        /// In preview mode, we do not actually translate the nodes, but only
        /// figure out what Prims will be exported, and where in the hierarchy.
        bool preview = false;
    };

    /**
     * \brief Holder for translation operations to execute, based on the success of a given predicate.
     */
    template <typename ReturnType, typename... InputTypes> struct TranslationOperation
    {
        /**
         * \brief Definition for the type of the predicate identifying if the translation operation should be
         * applied to the provided 3ds Max Object.
         */
        using selector_t = std::function<bool(Object*)>;
        /**
         * \brief Definition for the type of the translation operation to execute if the predicate elected to
         * operate on the provided 3ds Max Object.
         */
        using operation_t = std::function<ReturnType(InputTypes...)>;

        /**
         * \brief Default constructor.
         */
        TranslationOperation() = default;

        /**
         * \brief Constructor.
         * \param selectorPredicate Predicate used to elect if the given translation operation should be applied to
         * the provided 3ds Max object.
         * \param operation Translation operation to apply to the provided 3ds Max object, if the predicate elected it
         * for processing.
         */
        TranslationOperation(
            const selector_t&  selectorPredicate,
            const operation_t& operation) noexcept
            : AppliesToObject { selectorPredicate }
            , Execute { operation }
        {
            // Nothing to do.
        }

        /**
         * \brief Predicate to elect if the provided USD Prim should be selected for processing.
         */
        selector_t AppliesToObject { [](Object*) { return false; } };
        /**
         * \brief Translation operation to apply to the provided USD Prim, if the predicate elected it for
         * processing.
         */
        operation_t Execute { [](InputTypes...) { return nullptr; } };
    };

    /**
     * \brief Type definition for translation operations to be performed to convert 3ds Max content into USD content.
     */
    using translationOperation_t
        = TranslationOperation<MaxUsd::PrimDefVectorPtr, const TranslationContext&>;

    /**
     * \brief Type definition for additional translation operations performed after 3ds Max object converted into USD Prims
     */
    using translationPrimConfigurator_t
        = TranslationOperation<void, const TranslationContext&, pxr::UsdPrim&>;

protected:
    /**
     * \brief Start the scene export process to usd
     * \param writeJobContext The write job context (options, stage, target file, etc.)
     * \param primToNode A map of prims to their source nodes to be filled. Used to keep track of what prim was exported
     * from what node.
     * \param primsToMaterialBind The method will fill this set with the prims on which we should do material assignment
     * later in the export process.
     * \param progress Reference to the progress bar, to report progress with, as this can be a lengthy operation.
     * \return true if operation completed, false if operation failed or cancelled
     */
    bool BuildStageFromMaxNodes(
        pxr::MaxUsdWriteJobContext&                                     writeJobContext,
        pxr::MaxUsdExportChaserRegistry::FactoryContext::PrimToNodeMap& primToNode,
        pxr::TfHashSet<pxr::SdfPath, pxr::SdfPath::Hash>&               primsToMaterialBind,
        MaxProgressBar&                                                 progress);

    /**
     * \brief Processes a 3ds Max node for export to the USD Stage, using the provided translation context.
     * Will setup the prims in the stage and queue time dependent work (writing properties and
     * transforms) in the passed AnimnExportTask (indeed, this work is delayed to make sure we only
     * evaluate each 3dsMax object once at a certain time).
     * \param context The context of the node translation operation.
     * \param buidOptions Build configuration options to use during the translation process.
     * \param primCreators The prim creators to be used to convert the node.
     * \param primConfigurators The object configurators to be used to convert the node.
     * \param stage USD Stage into which to add the new prim(s).
     * \param doAssignMaterial True if the prim create for the 3dsMax node should be considered for material assignement,
     * this is from the implementation of the prim writer. IMPORTANT: In the case of instanced
     * objects, only the first instance may return doAssignMaterial = true, as subsequent instances
     * do not need to involve the prim writer at all. It is the responsibility of the caller to
     * handle this case.
     * \param animExportTask Time samples export task - where time-dependent work will be queued up so it can later
     * be batched with other work which need to be run at the same 3dsMax time. Essentially writing
     * object properties & transforms.
     * \return A vector of the created USD prims for the 3dsMax node (or that would be created if in preview mode).
     * The first prim in the vector is the root prim for the node.
     */
    MaxUsd::PrimDefVectorPtr ProcessNode(
        const TranslationContext&                                          context,
        const pxr::MaxUsdWriteJobContext&                                  jobContext,
        const std::vector<USDSceneBuilder::translationPrimConfigurator_t>& primConfigurators,
        const pxr::UsdStageRefPtr&                                         stage,
        bool&                                                              doAssignMaterial,
        MaxUsd::AnimExportTask&                                            animExportTask);

    /**
     * \brief Checks if anything in the hierarchy starting at a given node should be exported.
     * \param node The top-most node of the hierarchy. The function is called recursively on the
     * node's children, and a cache is maintained to avoid unnecessary traversals.
     * \param buildOptions The USD scene builder options.
     * \return True if the hierarchy contains objects which should be exported, false otherwise.
     */
    bool HasExportableDescendants(INode* node, const USDSceneBuilderOptions& buildOptions);

    /**
     * \brief Writes all the prims required to translate a Max node to USD. If the node's object has an offset,
     * we will need an extra xform prim, so that the object's offset is not propagated to children
     * nodes.  If the object can be exported as an instanceable prim, the function will create the
     * appropriate class prim and xform prim referencing the class prim.
     * \param createObjectPrim The translation function to use for the node's object itself.  The function take
     * 2 parameters, the translation context for the node and a boolean that indicate if the
     * translated prim should include the object transform.
     * \param context The translation context for the node. If in preview, we are just gathering all the prims that
     * will be written to, but not actually writing to them - we are previewing what we will write
     * to - knowing this we can later pre-create in all prims (without their properties) in a single
     * SdfChange block, to avoid costly notifications being sent out.
     * \param objectPrimSuffix The suffix to use in the object's prim name if it is created. The full name of that
     * \param xformRequirement Specifies if an extra Xform needs to be created to encode the node's transform, or if
     * it can be applied onto the object's prim directly. This is a requirement from the prim
     * writer, additional requirements may come into play forcing the creation of the Xform, even if
     * the prim writer itself doesn't need it (for example when using instancing).
     * \param instancingRequirement Whether or not default instancing should be used (i.e. instanced 3dsMax nodes would
     * generate instanced prims).
     * This will be ignored for node that are exported as instances, in which case the prim will
     * always be created if the offset transform is not identity.
     * \parm rootPrim The root prim we are exporting to. This may be useful in case we need to create prims outside
     * the hierarchy, but that should still be under the targeted root prim for export. For example,
     * prototype prims.
     * \return A vector of definitions of prims created from the node (or that would be created if in preview mode).
     * The first prim in the vector is the root prim for the node.
     */
    MaxUsd::PrimDefVectorPtr WriteNodePrims(
        std::function<MaxUsd::PrimDef(const TranslationContext&, bool applyOffsetTransform)>
                                             createObjectPrim,
        const TranslationContext&            context,
        const std::string&                   objectPrimSuffix,
        const MaxUsd::XformSplitRequirement& xformRequirement,
        const MaxUsd::InstancingRequirement& instancingRequirement,
        const pxr::SdfPath&                  rootPrim);

    /**
     * \brief Translate USD round-trip attributes stored as Max custom attributes
     * \param context The context of the translation operation.
     * \param translatedPrim the usd prim being translated to
     */
    virtual void ConfigureUsdAttributes(
        const TranslationContext& translationContext,
        pxr::UsdPrim&             translatedPrim);

    /**
     * \brief Configure the USD Kind for a node's exported prim - only do so if none already authored.
     * \param node The node we are exporting the prim from
     * \param translatedPrim The usd prim being translated to
     */
    void ConfigureKind(INode* node, pxr::UsdPrim& translatedPrim);

    /**
     * \brief Return the number of nodes to be exported for the given buildOptions
     * \param buildOptions Build configuration options to use during the translation process.
     * \return number of nodes that will be exported
     */
    int GetNumberOfNodeToExport(const USDSceneBuilderOptions& buildOptions);

    /**
     * \brief Return whether or not the parent prim should be used to export an instance.
     * We want to reuse the parent prim in certain situations to avoid creating extra layers of
     * prims when round-tripping usd data inside 3ds Max multiple times.
     * \param context The context of the translation operation.
     * \return True if the parent prim should be reuse to export the instance node.
     */
    bool ReuseParentPrimForInstancing(const TranslationContext& context);

private:
    /// Class ID of the Mesh type to select for translation operations:
    const Class_ID MESH_TYPE_ID { TRIOBJ_CLASS_ID, 0 };
    /// Class ID of the Camera type to select for translation operations:
    const Class_ID CAMERA_TYPE_ID { SIMPLE_CAM_CLASS_ID, 0 };
    /// Class ID of the Light type to select for translation operations:
    const Class_ID PHOTOMETRIC_LIGHT_TYPE_ID { LIGHTSCAPE_LIGHT_CLASS };
    /// Class ID of the Dummy type to select for translation operations:
    const Class_ID DUMMY_TYPE_ID { DUMMY_CLASS_ID, 0 };
    /// Class ID of the PointHelper type to select for translation operations:
    const Class_ID POINTHELPER_TYPE_ID { POINTHELP_CLASS_ID, 0 };

    /// Reference to the Core Interface to use to interface with 3ds Max:
    Interface17* coreInterface { GetCOREInterface17() };

    // Map of instanced Node to usd class Prim
    std::map<INode*, pxr::SdfPath> maxNodeToClassPrimMap;
    MaxUsd::UniqueNameGenerator    classPrimBaseNameGenerator;

    // Explicit set of nodes to be exported. Used when exporting the selection or from a node list.
    // Remains empty if exporting the whole scene.
    std::unordered_set<INode*> nodesToExportSet;

    // Cached maintained by HasExportableDescendants() to avoid extra scene graph
    // traversals. For each node in the map, the boolean value specifies whether or
    // not itself, or any of its descendants should be exported.
    std::map<INode*, bool> hasExportableDescendantsMap;

    // Instance to prototype prim map. We collect this during export so that we set up instancing
    // all at once in a single SdfChangeBlock at the end.
    pxr::TfHashMap<pxr::SdfPath, pxr::SdfPath, pxr::SdfPath::Hash> instanceToPrototype;
    // For instanced 3dsmax objects, we only run prim writers once, when exporting the first
    // instance. At this time, the prim writer tells us if we need to perform material assignment to
    // the prim it creates. We only get this info when exporting the first instance, but the answer
    // is the same for all instances. Therefor, we need to keep track of it, so we know if we need
    // to assign a material, when we hit further instances. This set hold the path of
    // prototype/class prims which require material assignment.
    pxr::TfHashSet<pxr::SdfPath, pxr::SdfPath::Hash> prototypeMaterialReq;

    /**
     * \brief Resets state data used during node export passes. We currently split the export in multiple passes,
     * for performance. The first only creates the prims. The second sets up the prim properties.
     * Some data held by the USDSceneBuilder needs to be reset between the passes.
     */
    void PrepareExportPass();
};

} // namespace MAXUSD_NS_DEF