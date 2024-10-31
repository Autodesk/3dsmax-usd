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

#include <pxr/imaging/hdx/pickTask.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>

#include <Graphics/ICamera.h>

#include <notify.h>

class HitRegion;
class QOpenGLDebugMessage;

/**
 * \brief This class enables picking within a USD Stage leveraging the UsdImagingGLEngine.
 */
class USDPickingRenderer
{
public:
    struct HitInfo
    {
        pxr::SdfPath primPath;
        pxr::SdfPath instancerPath;
        int          instanceIndex = 0;
        Point3       hitPoint;
    };

    /**
     * \brief Constructor.
     * \param stage The stage that will be considered in the picking operation.
     */
    USDPickingRenderer(const pxr::UsdStageWeakPtr stage);

    /**
     * \brief Destructor
     */
    ~USDPickingRenderer();

    /**
     * \brief Perform's hit testing.
     * \param stageTransform Root transform to apply to the stage.
     * \param camera Reference to the max camera used for picking.
     * \param hitRegion The 3dsMax hit region - describes the area to use for hit testing.
     * We only really support point and rectangle selection (no deep selection). Other modes,
     * like lasso, circle, etc. Are approximated using the encapsulating rectangles.
     * \param windowSize Window information.
     * \param wireframe Whether we are picking in a wireframe viewport or not.
     * \param displayProxy Whether or not the proxy purpose should be considered.
     * \param displayGuide Whether or not the guide purpose should be considered.
     * \param displayRender Whether or not the render purpose should be considered.
     * \param pickTarget What is being picked (prims, points, edges, etc..)
     * \param time Time used to perform hit testing operation.
     * \return Hit information. Whether or not a prim was picked, and a what distance.
     */
    std::vector<HitInfo> Pick(
        const pxr::GfMatrix4d&                 stageTransform,
        const MaxSDK::Graphics::CameraPtr&     camera,
        const MaxSDK::Graphics::RectangleSize& windowSize,
        const HitRegion&                       hitRegion,
        pxr::UsdImagingGLDrawMode              drawMode,
        bool                                   displayProxy,
        bool                                   displayGuide,
        bool                                   displayRender,
        const pxr::TfToken&                    pickTarget,
        const pxr::UsdTimeCode&                time,
        const pxr::SdfPathVector&              excludedPaths);

    /**
     * \brief Invalidates the internal renderer used for picking.
     */
    void InvalidateRenderer();

private:
    /**
     * \brief Derived Usd imaging engine. Adds support for area selection of multiple prims.
     * Note : this is not "deep selection", i.e. only visible prims can be hit.
     */
    class MaxUsdImagingGLEngine : public pxr::UsdImagingGLEngine
    {
    public:
        // This overload is exactly the same as pxr::UsdImagingGLEngine::TestIntersection except it
        // exposes the resolve mode.
        bool TestIntersection(
            const pxr::GfMatrix4d&               viewMatrix,
            const pxr::GfMatrix4d&               projectionMatrix,
            const pxr::UsdPrim&                  root,
            const pxr::UsdImagingGLRenderParams& params,
            pxr::GfVec3d*                        outHitPoint,
            pxr::GfVec3d*                        outHitNormal,
            pxr::SdfPath*                        outHitPrimPath,
            pxr::SdfPath*                        outHitInstancerPath,
            int*                                 outHitInstanceIndex,
            pxr::HdInstancerContext*             outInstancerContext,
            const pxr::TfToken&                  resolveMode);

        bool TestAreaIntersection(
            const pxr::GfMatrix4d&               viewMatrix,
            const pxr::GfMatrix4d&               projectionMatrix,
            const pxr::UsdPrim&                  root,
            const pxr::UsdImagingGLRenderParams& params,
            const pxr::TfToken&                  pickTarget,
            pxr::HdxPickHitVector&               outHits);

        void SetExcludePaths(const pxr::SdfPathVector& excludePaths);

        pxr::UsdImagingDelegate* GetSceneDelegate() const { return _GetSceneDelegate(); }

    protected:
        // Need a dedicated render collection for point snapping to use the point representation.
        pxr::HdRprimCollection _pointSnappingCollection { pxr::HdTokens->geometry,
                                                          pxr::HdReprSelector(
                                                              pxr::HdReprTokens->refined,
                                                              pxr::TfToken(),
                                                              pxr::HdReprTokens->points),
                                                          pxr::SdfPath::AbsoluteRootPath() };
    };

    /**
     * \brief Responding to a request to invalidate the render.
     * \param param Notification param, carries the USDPickingRenderer.
     * \param info Notification info. Not used.
     */
    static void ResetUSDRenderer(void* param, NotifyInfo* info);
    /**
     * \brief QOpenGl debug message logging.
     * \param message The message to log.
     */
    void MessageLogged(const QOpenGLDebugMessage& message);

    /// The stage being picked.
    pxr::UsdStageWeakPtr stage;
    /// Window size info.
    std::size_t width, height;
    /// The GL renderer used for picking.
    std::unique_ptr<MaxUsdImagingGLEngine> usdImagingRenderer;
    /// Flag for invalidating the renderer.
    bool invalidateRenderer = true;
};