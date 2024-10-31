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

#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/dictionary.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/path.h>

#include <MaxUsd.h>
#include <vector>

class QJsonObject;

namespace MAXUSD_NS_DEF {

namespace DictUtils {

/// \brief Extracts a bool at \p key from \p userArgs, or false if it can't extract.
MaxUSDAPI bool ExtractBoolean(const PXR_NS::VtDictionary& userArgs, const PXR_NS::TfToken& key);

/// \brief Extracts a pointer at \p key from \p userArgs, or nullptr if it can't extract.
MaxUSDAPI PXR_NS::UsdStageRefPtr
          ExtractUsdStageRefPtr(const PXR_NS::VtDictionary& userArgs, const PXR_NS::TfToken& key);

/// \brief Extracts a double at \p key from \p userArgs, or defaultValue if it can't extract.
MaxUSDAPI double ExtractDouble(
    const PXR_NS::VtDictionary& userArgs,
    const PXR_NS::TfToken&      key,
    double                      defaultValue);

/// \brief Extracts a string at \p key from \p userArgs, or "" if it can't extract.
MaxUSDAPI std::string
          ExtractString(const PXR_NS::VtDictionary& userArgs, const PXR_NS::TfToken& key);

/// \brief Extracts a token at \p key from \p userArgs.
/// If the token value is not either \p defaultToken or one of the
/// \p otherTokens, then returns \p defaultToken instead.
MaxUSDAPI PXR_NS::TfToken ExtractToken(
    const PXR_NS::VtDictionary&         userArgs,
    const PXR_NS::TfToken&              key,
    const PXR_NS::TfToken&              defaultToken,
    const std::vector<PXR_NS::TfToken>& otherTokens);

/// \brief Extracts an absolute path at \p key from \p userArgs, or the empty path if
/// it can't extract.
MaxUSDAPI PXR_NS::SdfPath
          ExtractAbsolutePath(const PXR_NS::VtDictionary& userArgs, const PXR_NS::TfToken& key);

/// \brief Extracts an vector<T> from the vector<VtValue> at \p key in \p userArgs.
/// Returns an empty vector if it can't convert the entire value at \p key into
/// a vector<T>.
template <typename T>
std::vector<T> ExtractVector(const PXR_NS::VtDictionary& userArgs, const PXR_NS::TfToken& key);

/// \brief Convenience function that takes the result of ExtractVector and converts it to a
/// TfToken::Set.
MaxUSDAPI PXR_NS::TfToken::Set
          ExtractTokenSet(const PXR_NS::VtDictionary& userArgs, const PXR_NS::TfToken& key);

// Implementation of the templated function declared above.
template <typename T>
std::vector<T> ExtractVector(const PXR_NS::VtDictionary& userArgs, const PXR_NS::TfToken& key)
{
    // Using declaration is necessary for the TF_ macros to compile as they assume
    // to be in that namespace.
    PXR_NAMESPACE_USING_DIRECTIVE

    // Check that vector exists.
    if (VtDictionaryIsHolding<std::vector<T>>(userArgs, key)) {
        std::vector<T> vals = VtDictionaryGet<std::vector<T>>(userArgs, key);
        return vals;
    }

    if (!VtDictionaryIsHolding<std::vector<VtValue>>(userArgs, key)) {
        TF_CODING_ERROR(
            "Dictionary is missing required key '%s' or key is "
            "not vector type",
            key.GetText());
        return std::vector<T>();
    }

    // Check that vector is correctly-typed.
    std::vector<VtValue> vals = VtDictionaryGet<std::vector<VtValue>>(userArgs, key);
    if (!std::all_of(vals.begin(), vals.end(), [](const VtValue& v) { return v.IsHolding<T>(); })) {
        TF_CODING_ERROR(
            "Vector at dictionary key '%s' contains elements of "
            "the wrong type",
            key.GetText());
        return std::vector<T>();
    }

    // Extract values.
    std::vector<T> result;
    for (const VtValue& v : vals) {
        result.push_back(v.UncheckedGet<T>());
    }
    return result;
}

/// \brief Attempts to get the value of the given key from the dictionary.
/// If the key is not found or if the type is not matching the value will be retrieved from the
/// guide. The Key must be present in the guide.
template <typename T>
const T& VtDictionaryGetWithDefault(
    const PXR_NS::VtDictionary& dict,
    const PXR_NS::VtDictionary& defaultDict,
    const std::string&          key)
{
    if (PXR_NS::VtDictionaryIsHolding<T>(dict, key)) {
        return PXR_NS::VtDictionaryGet<T>(dict, key);
    }
    return PXR_NS::VtDictionaryGet<T>(defaultDict, key);
}

/// \brief Coerces the dictionary entries to the type of the matching entries in the guide.
/// The goal is to ensure that the dictionary's entries have the same type as the guide.
/// If the type is incorrect, and if the method doesn't know how to convert it to the guide type,
/// the value from the guide will be assigned.
/// \param dict The dictionary to validate/coerce.
/// \param guide The guide dictionary to use for type enforcement.
MaxUSDAPI void CoerceDictToGuideType(pxr::VtDictionary& dict, const pxr::VtDictionary& guide);

/// \brief Converts a VtDictionary to a QJsonObject.
/// \param dic The dictionary to convert.
/// \param jsonObj The json object to populate.
MaxUSDAPI void VtDictToJson(const pxr::VtDictionary& dic, QJsonObject& jsonObj);

/// \brief Converts a json formatted string to a VtDictionary.
/// If the conversion fail an empty dictionary will be set.
/// \param stdStr The json formatted string to convert.
/// \param dict The VtDictionary to populate.
MaxUSDAPI void VtDictFromString(const std::string& stdStr, pxr::VtDictionary& dict);

} // namespace DictUtils

} // namespace MAXUSD_NS_DEF