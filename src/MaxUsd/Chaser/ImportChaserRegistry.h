//
// Copyright 2021 Apple
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
// © 2023 Autodesk, Inc. All rights reserved.
//
#pragma once

#include "ImportChaser.h"

#include <MaxUsd/MaxUSDAPI.h>
#include <MaxUsd/Translators/ReadJobContext.h>

#include <pxr/base/tf/declarePtrs.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/singleton.h>
#include <pxr/pxr.h>

#include <functional>

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_PTRS(MaxUsdImportChaserRegistry);

/// \class MaxUsdImportChaserRegistry
/// \brief Registry for chaser plugins.
class MaxUsdImportChaserRegistry : public TfWeakBase
{
public:
    /// \brief Holds data that can be accessed when constructing a
    /// MaxUsdImportChaser object.
    ///
    /// This class allows plugin code to only know about the context object
    /// during construction and only need to know about the data it is needs to
    /// construct.
    class FactoryContext
    {
    public:
        typedef TfHashMap<SdfPath, INode*, SdfPath::Hash> PrimToNodeMap;

        MaxUSDAPI FactoryContext(
            Usd_PrimFlagsPredicate& returnPredicate,
            MaxUsdReadJobContext&   context,
            const fs::path&         filename);

        /// \brief Returns the imported context.
        MaxUSDAPI MaxUsdReadJobContext& GetContext() const;

        /// \brief Returns the imported stage.
        ///
        /// It is safe for the MaxUsdImportChaser to save this return value and
        /// use it during it's execution.
        MaxUSDAPI UsdStagePtr GetStage() const;

        /// \brief Returns the current job args.
        ///
        /// It is safe for the MaxUsdImportChaser to save this return value by
        /// reference and use it during it's execution.
        MaxUSDAPI const MaxUsd::MaxSceneBuilderOptions& GetJobArgs() const;

        /// \brief Returns the filename of the imported stage
        ///
        /// It is safe for the MaxUsdImportChaser to save this return value and
        /// use it during it's execution.
        MaxUSDAPI const fs::path& GetFilename() const;

    private:
        MaxUsdReadJobContext& context;
        const fs::path&       filename;
    };

    typedef std::function<MaxUsdImportChaser*(const FactoryContext&)> FactoryFn;

    /// All the information registered for a chaser.
    struct ChaserInfo
    {
        TfToken   chaser;
        TfToken   niceName;
        TfToken   description;
        FactoryFn chaserFactory;

        ChaserInfo() = default;

        ChaserInfo(const TfToken& c, const TfToken& nn, const TfToken& cdsc, FactoryFn cf)
            : chaser(c)
            , niceName(nn)
            , description(cdsc)
            , chaserFactory(cf)
        {
        }
    };

    /// \brief Register a chaser factory.
    ///
    /// Please use the PXR_MAXUSD_DEFINE_CHASER_FACTORY instead of calling
    /// this directly.
    /// \param chaser the referenced name in the chaser option list on import
    /// \param niceName is the name displayed in the import options dialog.
    /// \param description is displayed as a tooltip in the import options dialog.
    ///
    MaxUSDAPI bool RegisterFactory(
        const std::string& chaser,
        const std::string& niceName,
        const std::string& description,
        FactoryFn          fn,
        bool               fromPython = false);

    /// Gets the chaser information
    static const ChaserInfo& GetChaserInfo(const TfToken& chaser)
    {
        return GetInstance()._GetChaserInfo(chaser);
    }

    /// \brief Creates a chaser using the factoring registered to \p name.
    MaxUSDAPI static MaxUsdImportChaserRefPtr
    Create(const std::string& name, const FactoryContext& context)
    {
        return GetInstance()._Create(name, context);
    }

    /// \brief Returns the names of all registered chasers.
    MaxUSDAPI static TfTokenVector GetAllRegisteredChasers()
    {
        return GetInstance()._GetAllRegisteredChasers();
    }

    MaxUSDAPI static MaxUsdImportChaserRegistry& GetInstance();

private:
    MaxUSDAPI TfTokenVector     _GetAllRegisteredChasers() const;
    MaxUSDAPI const ChaserInfo& _GetChaserInfo(const TfToken& chaser) const;
    MaxUSDAPI MaxUsdImportChaserRefPtr
    _Create(const std::string& name, const FactoryContext& context) const;

    MaxUsdImportChaserRegistry();
    ~MaxUsdImportChaserRegistry();
    friend class TfSingleton<MaxUsdImportChaserRegistry>;
};

#define PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_4(name, niceName, description, contextArgName) \
    static MaxUsdImportChaser* _ChaserFactory_##name(                                          \
        const MaxUsdImportChaserRegistry::FactoryContext&);                                    \
    TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdImportChaserRegistry, name)                            \
    {                                                                                          \
        MaxUsdImportChaserRegistry::GetInstance().RegisterFactory(                             \
            #name, niceName, description, &_ChaserFactory_##name);                             \
    }                                                                                          \
    MaxUsdImportChaser* _ChaserFactory_##name(                                                 \
        const MaxUsdImportChaserRegistry::FactoryContext& contextArgName)

#define PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_3(name, niceName, contextArgName) \
    PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_4(name, niceName, "", contextArgName)
#define PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_2(name, contextArgName) \
    PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_3(name, #name, contextArgName)
#define PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_1(name) \
    PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_2(name, ctx)
#define PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_0()                                        \
    static_assert(                                                                         \
        false,                                                                             \
        "Insufficient number of arguments for macro; expecting "                           \
        "PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY(<chaserid>, <nice_name>, <description>, " \
        "<context_arg_name>)");                                                            \
    PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_1(InvalidChaserDeclaration)

#define FUNC_CHOOSER_IMPORT(_f1, _f2, _f3, _f4, _f5, ...) _f5
#define FUNC_RECOMPOSER_IMPORT(argsWithParentheses)       FUNC_CHOOSER_IMPORT argsWithParentheses
#define CHOOSE_FROM_ARG_COUNT_IMPORT(...)           \
    FUNC_RECOMPOSER_IMPORT(                         \
        (__VA_ARGS__,                               \
         PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_4, \
         PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_3, \
         PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_2, \
         PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_1, ))
#define NO_ARG_EXPANDER_IMPORT()  , , , , PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_0
#define MACRO_CHOOSER_IMPORT(...) CHOOSE_FROM_ARG_COUNT_IMPORT(NO_ARG_EXPANDER_IMPORT __VA_ARGS__())

/// \brief define a factory for the chaser name.
/// /param name the chaser id to use when referenced in import options
/// /param niceName the chaser name displayed in the USD import UI
///                 (optional; chaser id is used if not provided)
/// /param description the description of chaser (optional)
/// /param contextArgName context argument of type MaxUsdImportChaserRegistry::FactoryContext.
///                       (optional; the argument is named `ctx` if not provided)
/// The following code block should return a MaxUsdImportChaser*.  There are no guarantees about
/// the lifetime of contextArgName.
// PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY_FULL(name, niceName, description, contextArgName)
#define PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY(...) MACRO_CHOOSER_IMPORT(__VA_ARGS__)(__VA_ARGS__)

PXR_NAMESPACE_CLOSE_SCOPE
