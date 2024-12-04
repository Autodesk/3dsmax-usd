//
// Copyright 2016 Pixar
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
// © 2022 Autodesk, Inc. All rights reserved.
//
#pragma once

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/MaxTokens.h>

PXR_NAMESPACE_OPEN_SCOPE

class MaxUsdWriteJobContext;

/// \brief Base class for all built-in and user-defined prim writers. Translates 3dsMax
/// nodes to USD prims.
///
/// The PrimWriter is only responsible for providing translation of the 3dsMax Object referenced by
/// the received Node. It should therefore not attempt to handle instancing, material assignment,
/// and the transform of the Node itself. Instancing is handled by the calling code - if an object
/// is instanced across multiple nodes, the PrimWriter is only called once, on the first node
/// referencing the instanced object. The required Xform prim hierarchy is already generated.
/// Similarly, the Node's transform is applied by the calling code, on the UsdGeomXformable prim
/// built by the PrimWriter, *after* it is run. If the USD prim is *not* a UsdGeomXformable, a
/// warning is raised, but it doesn't prevent the export from continuing.
class MaxUsdPrimWriter
{
public:
    /// \brief The level of support a writer can offer for a given context
    ///
    /// A basic writer that gives correct results across most contexts should
    /// report `Fallback`, while a specialized writer that really shines in a
    /// given context should report `Supported` when the context is right and
    /// `Unsupported` if the context is not as expected.
    enum class ContextSupport
    {
        Unsupported,
        Supported,
        Fallback
    };

    /**
     * \brief Constructor.
     * \param jobCtx The write job context (options, target file, stage, etc.)
     * \parm node The node that this writer will translate to a USD prim.
     */
    MaxUSDAPI MaxUsdPrimWriter(const MaxUsdWriteJobContext& jobCtx, INode* node);

    /**
     * \brief Destructor.
     */
    MaxUSDAPI virtual ~MaxUsdPrimWriter();

    /**
     * \brief The prim type you are writing to.
     * For performance reasons, all prims get created ahead of time in a single `SdfChangeBlock`.
     * This means the prim writers are not responsible for creating the prims. The type specified
     * here is mostly a hint for that first creation pass and, if required, it can be overridden
     * from the `Write()` method (by defining a prim at the same path with a different type). Unless
     * you always force the creation of an Xform prim (see `RequireXformPrim()`), you should return
     * an Xformable type here; otherwise, it is not possible to apply the node's transform onto the
     * prim later (an error will be raised in this scenario). \return The prim's type.
     */
    MaxUSDAPI virtual TfToken GetPrimType() { return pxr::MaxUsdPrimTypeTokens->Xform; };

    /**
     * \brief Responsible for writing the prim's attribute for the given context. This is where the translation from
     * the 3dsMax node to the USD prim happens.
     * \param targetPrim The prim we are writing to.
     * \param applyOffsetTransform Whether or not the object offset transform should be applied onto the prim. Can only
     * be true if HandlesObjectOffsetTransform() returned true, otherwise it will always be false.
     * \param time The 3dsMax time and matching USD timecode at which the write takes place.
     * \return True if the write operation was successful, false otherwise.
     */
    MaxUSDAPI virtual bool
    Write(UsdPrim& targetPrim, bool applyOffsetTransform, const MaxUsd::ExportTime& time)
    {
        return false;
    }

    /**
     * \brief Called after Write has been called to all other nodes. Useful place to write information for the prim that
     * are dependent on other prims on the stage.
     * \param targetPrim The prim we are writing to.
     * \return True if the write operation was successful, false otherwise.
     */
    MaxUSDAPI virtual bool PostExport(UsdPrim& targetPrim) { return true; }

    /**
     * \brief Returns the name of this prim writer.
     * \return The writer's name.
     */
    MaxUSDAPI virtual WStr GetWriterName() { return WStr(); };

    /**
     * \brief In a few scenarios, you need two prims to properly represent an INode. One for its transform, and one
     * for the object it references (for example, in the case of a non-identity object offset
     * transform, it must not inherit the transform, so you cannot use a single prim for the INode).
     * When you do need to perform such a split, the object's prim has the same name as the node's
     * prim, with an added suffix - the string returned here.
     * \return A suffix for the object prim.
     */
    MaxUSDAPI virtual TfToken GetObjectPrimSuffix() { return TfToken("Object"); };

    /**
     * \brief Returns the name that should be used for the prim. The base implementation should be sufficient in most
     * cases, unless prim writers want to customize the prim's name. If so, it is their
     * responsibility to ensure that the given name is unique amongst siblings.
     * \param suggestedName A suggested name, from the node's name, and ensured unique amongst siblings.
     * \return The name that should be used (will be part of the prim's path /foo/bar/{Prim name}
     */
    MaxUSDAPI virtual std::string GetPrimName(const std::string& suggestedName)
    {
        return suggestedName;
    };

    /**
     * \brief Returns the requirement to split the object from its transform in the scene.
     * - ForOffsetObjects : This should be the case for most objects, this means we require an Xform
     * if an object offset is applied to the object. Indeed, object offset transforms should not be
     * inherited, so we need an Xform prim to encode the node's transform (the children of the node
     * will export to children of this prim), and another for the object itself, which will be
     * exported to a prim under that Xform. It will contain the object offset transform, and it will
     * not have children.
     * - Always :For cases where we always need to have a seperate prim for the node's transform.
     * For example if we are exporting to a gprim, which, unless we explicitely allow it, should not
     * be nested. This could also be the case if we need to add an inherent transform to the
     * object's prim, part of the translation, and we never want that transform to be inherited.
     * - Never : To be used if we know we never want to split the node's transform from its object.
     * For example we could be baking the object offset transform into the geometry itself - in this
     * scenario, we avoid the *need of an extra Xform entirely.
     * \return The Xform split requirement for this object.
     */
    MaxUSDAPI virtual MaxUsd::XformSplitRequirement RequiresXformPrim()
    {
        return MaxUsd::XformSplitRequirement::ForOffsetObjects;
    };

    /**
     * \brief Whether or not we want to manually handle the object offset transform in the Write(). If we
     * return true here, then the applyOffsetTransform argument of Write() can sometimes be set to
     * true.
     * \return True if the prim writer intends to handle the object offset transform, false otherwise.
     */
    MaxUSDAPI virtual bool HandlesObjectOffsetTransform() { return false; };

    /**
     * \brief Returns the material assignment requirement for this node. Some prim writers may not want the prims
     * they produce to be assigned the source node's material automatically.
     * \return The material assignment requirement.
     */
    MaxUSDAPI virtual MaxUsd::MaterialAssignRequirement RequiresMaterialAssignment()
    {
        return MaxUsd::MaterialAssignRequirement::Default;
    };

    /**
     * \brief Returns the instancing requirement for this Prim Writer. Some Prim Writers may want to handle instancing
     * themselves, or disable it entirely.
     * \return The instancing requirement. If default, instancing is handled automatically. Otherwise, it is left to the
     * Prim writer to decide what to do.
     */
    MaxUSDAPI virtual MaxUsd::InstancingRequirement RequiresInstancing()
    {
        return MaxUsd::InstancingRequirement::Default;
    };

    /**
     * \brief Returns the validity interval of the data that this writer exports from a certain 3dsMax time value.
     * From this information, we can figure out frames need to be exported from this object. For
     * example, a writer which does not export any animated data may override this and return the
     * FOREVER interval, i.e. what is exported in the first call to Write() will be valid at every
     * other time. The default implementation returns the validity interval of the object itself at
     * the given time.
     * \param time The 3dsMax time value.
     * \return The export's validity interval.
     */
    MaxUSDAPI virtual Interval GetValidityInterval(const TimeValue& time);

    MaxUSDAPI const UsdStageRefPtr& GetUsdStage() const;
    MaxUSDAPI const std::string&          GetFilename() const;
    MaxUSDAPI virtual boost::python::dict GetNodesToPrims() const;

protected:
    /// Gets the current global export args in effect.
    MaxUSDAPI const MaxUsd::USDSceneBuilderOptions& GetExportArgs() const;
    MaxUSDAPI const MaxUsdWriteJobContext&          GetJobContext() const;
    MaxUSDAPI INode*                                GetNode() const;

private:
    const MaxUsdWriteJobContext& writeJobCtx;
    INode*                       node;
};

typedef std::shared_ptr<MaxUsdPrimWriter> MaxUsdPrimWriterSharedPtr;

PXR_NAMESPACE_CLOSE_SCOPE
