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
#define _USE_MATH_DEFINES
#define _MATH_DEFINES_DEFINED

#include "USDPickingRenderer.h"

#include <RenderDelegate/HdLightGizmoSceneIndexFilter.h>
#include <RenderDelegate/HdMaxLightGizmoMeshAccess.h>

#include <MaxUsd/MaxUSDAPI.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usdImaging/usdImaging/delegate.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>
#include <pxr/usdImaging/usdImagingGL/renderParams.h>

#include <QtGui/QOffscreenSurface>
#include <QtWidgets/QApplication>
#include <maxapi.h>
#include <notify.h>
#ifdef IS_MAX2025_OR_GREATER
#include <QtOpenGL/QOpenGLDebugLogger>
#else
#include <QtGui/QOpenGLDebugLogger>
#endif

#include "MaxUsd/Utilities/HydraUtils.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>

// Set to true to enable OpenGL logging.
static const bool openGlLogging = false;

namespace {
QScopedPointer<QOpenGLContext>    glContext;
QScopedPointer<QOffscreenSurface> glSurface;
bool                              meetsMinimumRequirements = false;
const auto                        translationContext = "USDPickingRenderer";
} // namespace

USDPickingRenderer::USDPickingRenderer(const pxr::UsdStageWeakPtr stage)
    : stage(stage)
{
    if (!glSurface || !glSurface->isValid()) {
        QSurfaceFormat format;
        format.setDepthBufferSize(32);
        format.setMajorVersion(4);
        format.setMinorVersion(5);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setOption(QSurfaceFormat::DebugContext);

        glContext.reset(new QOpenGLContext());
        glContext->setFormat(format);
        glContext->create();

        glSurface.reset(new QOffscreenSurface());
        glSurface->setFormat(format);

        glSurface->create();

        if (openGlLogging) {
            glContext->makeCurrent(glSurface.get());

            auto logger = new QOpenGLDebugLogger(glContext.get());

            logger->connect(
                logger, &QOpenGLDebugLogger::messageLogged, [](const QOpenGLDebugMessage& message) {
                    qInfo() << message;
                });

            if (logger->initialize()) {
                std::cout << "OpenGL logger initialized" << std::endl;
                logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
                logger->enableMessages();
                // Disable NVidia performance spam APISource 131185 :
                // Buffer object 1 (bound to GL_ARRAY_BUFFER_ARB, usage hint is GL_STATIC_DRAW) will
                // use VIDEO memory as the source for buffer object operations.
                logger->disableMessages(QVector<GLuint> { 131185 });
            } else {
                std::cout << "!!!OpenGL logger not initialized!!!" << std::endl;
            }

            glContext->doneCurrent();
        }
    }

    glContext->makeCurrent(glSurface.get());

    // On first execution, try to detect missing OpenGL requirements.
    // Inspired from pxr::HgiGLMeetsMinimumRequirements()
    static std::once_flag checkOpenGlVersionOnce;
    std::call_once(checkOpenGlVersionOnce, []() {
        std::string openGlVersionStr = "None";
        if (!glContext.isNull() && glContext->isValid()) {
            openGlVersionStr
                = reinterpret_cast<const char*>(glContext->functions()->glGetString(GL_VERSION));

            int         glVersion = 0;
            const char* dot = strchr(openGlVersionStr.c_str(), '.');
            if ((dot && dot != openGlVersionStr)) {
                // GL_VERSION = "4.5.0 <vendor> <version>"
                //              "4.1 <vendor-os-ver> <version>"
                //              "4.1 <vendor-os-ver>"
                int major = std::max(0, std::min(9, *(dot - 1) - '0'));
                int minor = std::max(0, std::min(9, *(dot + 1) - '0'));
                glVersion = major * 100 + minor * 10;
            }
            meetsMinimumRequirements = (glVersion >= 450);
        }

        if (!meetsMinimumRequirements) {
            // Picking will be disable completely, warn the user. It will still be possible to
            // select the object via the icon.
            auto title = QApplication::translate(
                translationContext, "Missing Requirements for USD in 3dsMax.");
            auto msg = QApplication::translate(
                           translationContext,
                           "Picking the USD stage geometry in the viewport has been disabled "
                           "(picking via the USD stage icon "
                           "is still possible). OpenGL version 4.5 is required and version \"%1\" "
                           "has been detected. Try "
                           "updating your video card drivers and reloading 3ds Max. Note that "
                           "overriding the QT_OPENGL environment "
                           "variable can interfere with OpenGL support.")
                           .arg(QString::fromStdString(openGlVersionStr));
            GetCOREInterface()->Log()->LogEntry(
                SYSLOG_ERROR, TRUE, title.toStdWString().c_str(), msg.toStdWString().c_str());
        }
    });

    glContext->doneCurrent();

    RegisterNotification(&USDPickingRenderer::ResetUSDRenderer, this, NOTIFY_POST_IMPORT);
    RegisterNotification(&USDPickingRenderer::ResetUSDRenderer, this, NOTIFY_POST_SCENE_RESET);
}

USDPickingRenderer::~USDPickingRenderer()
{
    glContext->makeCurrent(glSurface.get());
    usdImagingRenderer.reset();
    glContext->doneCurrent();

    UnRegisterNotification(&USDPickingRenderer::ResetUSDRenderer, this, NOTIFY_POST_IMPORT);
    UnRegisterNotification(&USDPickingRenderer::ResetUSDRenderer, this, NOTIFY_POST_SCENE_RESET);
}

std::vector<USDPickingRenderer::HitInfo> USDPickingRenderer::Pick(
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
    const pxr::SdfPathVector&              excludedPaths)
{
    if (!meetsMinimumRequirements) {
        return {};
    }

    const auto glContextGuard = MaxUsd::MakeScopeGuard(
        []() { glContext->makeCurrent(glSurface.get()); }, []() { glContext->doneCurrent(); });

    if (invalidateRenderer) {
        usdImagingRenderer.reset(new MaxUsdImagingGLEngine());
        invalidateRenderer = false;

        // In > 0.23.11, inject a scene index filter to display (and pick) light gizmos.
#if PXR_VERSION >= 2311
        auto sceneDelegate = usdImagingRenderer->GetSceneDelegate();
        auto terminalSceneIndex = pxr::TfDynamic_cast<pxr::HdFilteringSceneIndexBaseRefPtr>(
            sceneDelegate->GetRenderIndex().GetTerminalSceneIndex());

        if (terminalSceneIndex) {
            if (auto mergingSceneIndex
                = MaxUsd::FindTopLevelMergingSceneIndex(terminalSceneIndex)) {
                // Swap the USD scene index for the light gizmo filter, with the USD scene index as
                // input.
                auto base = mergingSceneIndex->GetInputScenes()[0];
                mergingSceneIndex->RemoveInputScene(base);
                auto filter = pxr::HdLightGizmoSceneIndexFilter::New(
                    base, std::make_shared<HdMaxLightGizmoMeshAccess>());
                mergingSceneIndex->AddInputScene(filter, pxr::SdfPath { "/" });
            }
        }
#endif
    }

    // Setup render parameters for picking. Disable most things, as not required.
    pxr::UsdImagingGLRenderParams params;
    params.frame = time;
    params.drawMode = drawMode;
    params.enableSceneMaterials = false;
    params.enableLighting = false;
    params.enableSceneLights = false;

    params.showGuides = displayGuide;
    params.showProxy = displayProxy;
    params.showRender = displayRender;

    usdImagingRenderer->SetRootTransform(stageTransform);
    usdImagingRenderer->PrepareBatch(stage->GetPseudoRoot(), params);

    pxr::GfMatrix4d ovm, opm;
    ovm = MaxUsd::ToUsd(camera->GetViewMatrix());
    opm = MaxUsd::ToUsd(camera->GetProjectionMatrix());

    Point2 pickWindowSize;
    Point2 pickPosition;

    bool areaHitTesting = false;

    // Figure out the pick window size and the pick position.
    // Both are stored as percentages of the window size.
    switch (hitRegion.type) {
    case POINT_RGN: {
        // 3x3 pixels.
        double pixelSize = 1.0 + hitRegion.epsilon * 2;
        pickWindowSize = Point2(pixelSize / windowSize.cx, pixelSize / windowSize.cy);
        pickPosition = Point2(
            double(hitRegion.pt.x) / windowSize.cx, double(hitRegion.pt.y) / windowSize.cy);
        break;
    }
    case RECT_RGN: {
        const auto sizeX = double(hitRegion.rect.right - hitRegion.rect.left) / windowSize.cx;
        const auto sizeY = double(hitRegion.rect.bottom - hitRegion.rect.top) / windowSize.cy;
        pickWindowSize = Point2(sizeX, sizeY);
        const auto xCenter
            = double(hitRegion.rect.left + hitRegion.rect.right) / 2.0 / windowSize.cx;
        const auto yCenter
            = double(hitRegion.rect.top + hitRegion.rect.bottom) / 2.0 / windowSize.cy;
        pickPosition = Point2(xCenter, yCenter);
        areaHitTesting = true;
        break;
    }
    case CIRCLE_RGN: {
        // We do not properly support circle selection - create an area fully encapsulating the
        // circle...
        const auto sizeX = double(hitRegion.circle.r * 2) / windowSize.cx;
        const auto sizeY = double(hitRegion.circle.r * 2) / windowSize.cy;
        pickWindowSize = Point2(sizeX, sizeY);
        const auto xCenter = double(hitRegion.circle.x) / windowSize.cx;
        const auto yCenter = double(hitRegion.circle.y) / windowSize.cy;
        pickPosition = Point2(xCenter, yCenter);
        areaHitTesting = true;
        break;
    }
    case FENCE_RGN: {
        long maxX = LONG_MIN;
        long minX = LONG_MAX;
        long maxY = LONG_MIN;
        long minY = LONG_MAX;
        // We do not properly support fence selection - create an area fully encapsulating the
        // fence... In fence regions, epsilon is filled with the number of points.
        for (int i = 0; i < hitRegion.epsilon; i++) {
            const auto x = hitRegion.pts[i].x;
            const auto y = hitRegion.pts[i].y;
            maxX = std::max(x, maxX);
            minX = std::min(x, minX);
            maxY = std::max(y, maxY);
            minY = std::min(y, minY);
        }

        // The area size, expressed as a percentage of the entire window size.
        const auto sizeX = double(maxX - minX) / windowSize.cx;
        const auto sizeY = double(maxY - minY) / windowSize.cy;
        pickWindowSize = Point2(sizeX, sizeY);

        // Pick position is the center of the area (again, percentage relative to the full window).
        const auto xCenter = double(minX + maxX) / 2.0 / windowSize.cx;
        const auto yCenter = double(minY + maxY) / 2.0 / windowSize.cy;
        pickPosition = Point2(xCenter, yCenter);
        areaHitTesting = true;
        break;
    }
    }

    // Change pickPosition from 0..1 to -1..1
    pickPosition *= 2.0;
    pickPosition -= Point2(1.0, 1.0);

    // Tweak projection matrix so it restrict frustum to the box given in parameter
    // Basically rescale x and y so the viewport is filled with the restricted view
    // applying the offset
    // Should also work with non-symmetric frustum, not tested thus.
    opm[0][0] /= pickWindowSize[0];
    opm[1][1] /= pickWindowSize[1];
    // Account for orthographic or perspective projection...
    if (camera->IsPerspective()) {
        opm[2][0] += pickPosition[0] / pickWindowSize[0];
        opm[2][1] -= pickPosition[1] / pickWindowSize[1];
    } else {
        opm[3][0] -= pickPosition[0] / pickWindowSize[0];
        opm[3][1] += pickPosition[1] / pickWindowSize[1];
    }

    usdImagingRenderer->SetExcludePaths(excludedPaths);

    std::vector<HitInfo> hitInfos;

    if (!areaHitTesting && pickTarget == pxr::HdxPickTokens->pickPrimsAndInstances) {
        pxr::GfVec3d hitPoint;
        pxr::GfVec3d worldSpaceNormalPoint;
        pxr::SdfPath primPath;
        pxr::SdfPath instancerPath;
        int          instanceIndex;

        pxr::HdInstancerContext instancerContext;
        if (usdImagingRenderer->TestIntersection(
                ovm,
                opm,
                stage->GetPseudoRoot(),
                params,
                &hitPoint,
                &worldSpaceNormalPoint,
                &primPath,
                &instancerPath,
                &instanceIndex,
                &instancerContext,
                pxr::HdxPickTokens->resolveNearestToCamera)) {
            if (!instancerContext.empty()) {
                instanceIndex = instancerContext.front().second;
            }
            hitInfos.push_back({ primPath,
                                 instancerPath,
                                 instanceIndex,
                                 Point3(hitPoint[0], hitPoint[1], hitPoint[2]) });
        }
    } else {
        pxr::HdxPickHitVector hits;
        usdImagingRenderer->TestAreaIntersection(
            ovm, opm, stage->GetPseudoRoot(), params, pickTarget, hits);
        hitInfos.reserve(hits.size());
        for (const auto& hit : hits) {
            hitInfos.push_back({ hit.objectId,
                                 hit.instancerId,
                                 hit.instanceIndex,
                                 Point3(
                                     hit.worldSpaceHitPoint[0],
                                     hit.worldSpaceHitPoint[1],
                                     hit.worldSpaceHitPoint[2]) });
        }
    }

    return hitInfos;
}

void USDPickingRenderer::InvalidateRenderer()
{
    // Store that we need to invalidate render on next frame
    invalidateRenderer = true;
}

bool USDPickingRenderer::MaxUsdImagingGLEngine::TestIntersection(
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
    const pxr::TfToken&                  resolveMode)
{
    // The code bellow is as-is from UsdImagingGLEngine::TestIntersection() with the exception that
    // it exposes the resolve mode as an argument.

    if (ARCH_UNLIKELY(!_renderDelegate)) {
        return false;
    }

    PrepareBatch(root, params);

    // XXX(UsdImagingPaths): This is incorrect...  "Root" points to a USD
    // subtree, but the subtree in the hydra namespace might be very different
    // (e.g. for native instancing).  We need a translation step.
    const pxr::SdfPathVector paths
        = { root.GetPath().ReplacePrefix(pxr::SdfPath::AbsoluteRootPath(), _sceneDelegateId) };
    _UpdateHydraCollection(&_intersectCollection, paths, params);

    _PrepareRender(params);

    pxr::HdxPickHitVector         allHits;
    pxr::HdxPickTaskContextParams pickParams;
    pickParams.resolveMode = resolveMode;
    pickParams.viewMatrix = viewMatrix;
    pickParams.projectionMatrix = projectionMatrix;
    pickParams.clipPlanes = params.clipPlanes;
    pickParams.collection = _intersectCollection;
    pickParams.outHits = &allHits;
    const pxr::VtValue vtPickParams(pickParams);

    _GetHdEngine()->SetTaskContextData(pxr::HdxPickTokens->pickParams, vtPickParams);
    _Execute(params, _taskController->GetPickingTasks());

    // Since we are in nearest-hit mode, we expect allHits to have
    // a single point in it.
    if (allHits.size() != 1) {
        return false;
    }

    pxr::HdxPickHit& hit = allHits[0];

    if (outHitPoint) {
        *outHitPoint = hit.worldSpaceHitPoint;
    }

    if (outHitNormal) {
        *outHitNormal = hit.worldSpaceHitNormal;
    }

    if (auto sceneDelegate = GetSceneDelegate()) {
        hit.objectId
            = sceneDelegate->GetScenePrimPath(hit.objectId, hit.instanceIndex, outInstancerContext);
        hit.instancerId = sceneDelegate->ConvertIndexPathToCachePath(hit.instancerId)
                              .GetAbsoluteRootOrPrimPath();
    }
#if PXR_VERSION >= 2311
    else {
        const pxr::HdxPrimOriginInfo info
            = pxr::HdxPrimOriginInfo::FromPickHit(_renderIndex.get(), hit);
        const pxr::SdfPath usdPath = info.GetFullPath();
        if (!usdPath.IsEmpty()) {
            hit.objectId = usdPath;
        }
    }
#endif

    if (outHitPrimPath) {
        *outHitPrimPath = hit.objectId;
    }
    if (outHitInstancerPath) {
        *outHitInstancerPath = hit.instancerId;
    }
    if (outHitInstanceIndex) {
        *outHitInstanceIndex = hit.instanceIndex;
    }

    return true;
}

bool USDPickingRenderer::MaxUsdImagingGLEngine::TestAreaIntersection(
    const pxr::GfMatrix4d&               viewMatrix,
    const pxr::GfMatrix4d&               projectionMatrix,
    const pxr::UsdPrim&                  root,
    const pxr::UsdImagingGLRenderParams& params,
    const pxr::TfToken&                  pickTarget,
    pxr::HdxPickHitVector&               outHits)
{
    if (ARCH_UNLIKELY(!_renderDelegate)) {
        return false;
    }

    PrepareBatch(root, params);

    // Below code is heavily inspired from UsdImagingGLEngine::TestIntersection().

    const pxr::SdfPathVector paths
        = { root.GetPath().ReplacePrefix(pxr::SdfPath::AbsoluteRootPath(), _sceneDelegateId) };

    _UpdateHydraCollection(&_intersectCollection, paths, params);
    _PrepareRender(params);

    pxr::HdxPickTaskContextParams pickParams;
    pickParams.pickTarget = pickTarget;

    const auto resolveMode = pickTarget == pxr::HdxPickTokens->pickPrimsAndInstances
        ? pxr::HdxPickTokens->resolveUnique
        : pxr::HdxPickTokens->resolveNearestToCenter;

    pickParams.resolveMode = resolveMode;
    pickParams.doUnpickablesOcclude = false;
    pickParams.viewMatrix = viewMatrix;
    pickParams.projectionMatrix = projectionMatrix;
    pickParams.clipPlanes = params.clipPlanes;

    // Need a special collection when picking points as the points repr is not generally used.
    if (pickTarget == pxr::HdxPickTokens->pickPoints) {
        pickParams.collection = _pointSnappingCollection;
    } else {
        pickParams.collection = _intersectCollection;
    }

    pickParams.outHits = &outHits;
    const pxr::VtValue vtPickParams(pickParams);

    _GetHdEngine()->SetTaskContextData(pxr::HdxPickTokens->pickParams, vtPickParams);
    _Execute(params, _taskController->GetPickingTasks());

    if (outHits.empty()) {
        return false;
    }

    const auto sceneDelegate = _GetSceneDelegate();

    // Adjust hit data to have proper prim paths as object IDs.
    for (auto& hit : outHits) {
        pxr::HdInstancerContext instancerContext;
        hit.objectId
            = sceneDelegate->GetScenePrimPath(hit.objectId, hit.instanceIndex, &instancerContext);
        // Map the prototype instance index to the instancer index.
        if (!instancerContext.empty()) {
            hit.instanceIndex = instancerContext.front().second;
        }
        hit.instancerId = sceneDelegate->ConvertIndexPathToCachePath(hit.instancerId)
                              .GetAbsoluteRootOrPrimPath();
    }
    return true;
}

void USDPickingRenderer::MaxUsdImagingGLEngine::SetExcludePaths(
    const pxr::SdfPathVector& excludePaths)
{
    _intersectCollection.SetExcludePaths(excludePaths);
    _pointSnappingCollection.SetExcludePaths(excludePaths);
}

void USDPickingRenderer::ResetUSDRenderer(void* param, NotifyInfo* info)
{
    static_cast<USDPickingRenderer*>(param)->InvalidateRenderer();
}

void USDPickingRenderer::MessageLogged(const QOpenGLDebugMessage& message)
{
    std::cout << message.message().toStdString() << std::endl;
}