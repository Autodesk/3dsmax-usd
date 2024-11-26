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
#include "VtDictionaryUtils.h"

#include <MaxUsd/Utilities/MaxSupportUtils.h>
#ifdef IS_MAX2025_OR_GREATER
#include <Geom/acolor.h>
#else
#include <acolor.h>
#endif

#if _MSVC_LANG > 201402L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#include <pxr/base/js/converter.h>
#include <pxr/base/js/json.h>

#include <QJsonArray>
#include <QJsonObject>

namespace MAXUSD_NS_DEF {

namespace DictUtils {

PXR_NAMESPACE_USING_DIRECTIVE

/// Extracts a bool at \p key from \p userArgs, or false if it can't extract.
bool ExtractBoolean(const VtDictionary& userArgs, const TfToken& key)
{
    if (!VtDictionaryIsHolding<bool>(userArgs, key)) {
        TF_CODING_ERROR(
            "Dictionary is missing required key '%s' or key is "
            "not bool type",
            key.GetText());
        return false;
    }
    return VtDictionaryGet<bool>(userArgs, key);
}

/// Extracts a pointer at \p key from \p userArgs, or nullptr if it can't extract.
UsdStageRefPtr ExtractUsdStageRefPtr(const VtDictionary& userArgs, const TfToken& key)
{
    if (!VtDictionaryIsHolding<UsdStageRefPtr>(userArgs, key)) {
        TF_CODING_ERROR(
            "Dictionary is missing required key '%s' or key is "
            "not pointer type",
            key.GetText());
        return nullptr;
    }
    return VtDictionaryGet<UsdStageRefPtr>(userArgs, key);
}

/// Extracts a double at \p key from \p userArgs, or defaultValue if it can't extract.
double ExtractDouble(const VtDictionary& userArgs, const TfToken& key, double defaultValue)
{
    if (VtDictionaryIsHolding<double>(userArgs, key))
        return VtDictionaryGet<double>(userArgs, key);

    // Since user dictionary can be provided from Python and in Python it is easy to
    // mix int and double, especially since value literal will take the simplest value
    // they can, for example 0 will be an int, support receiving the value as an integer.
    if (VtDictionaryIsHolding<int>(userArgs, key))
        return VtDictionaryGet<int>(userArgs, key);

    TF_CODING_ERROR(
        "Dictionary is missing required key '%s' or key is "
        "not double type",
        key.GetText());
    return defaultValue;
}

/// Extracts a string at \p key from \p userArgs, or "" if it can't extract.
std::string ExtractString(const VtDictionary& userArgs, const TfToken& key)
{
    if (!VtDictionaryIsHolding<std::string>(userArgs, key)) {
        TF_CODING_ERROR(
            "Dictionary is missing required key '%s' or key is "
            "not string type",
            key.GetText());
        return std::string();
    }
    return VtDictionaryGet<std::string>(userArgs, key);
}

/// Extracts a token at \p key from \p userArgs.
/// If the token value is not either \p defaultToken or one of the
/// \p otherTokens, then returns \p defaultToken instead.
TfToken ExtractToken(
    const VtDictionary&         userArgs,
    const TfToken&              key,
    const TfToken&              defaultToken,
    const std::vector<TfToken>& otherTokens)
{
    const TfToken tok(ExtractString(userArgs, key));
    for (const TfToken& allowedTok : otherTokens) {
        if (tok == allowedTok) {
            return tok;
        }
    }

    // Empty token will silently be promoted to default value.
    // Warning for non-empty tokens that don't match.
    if (tok != defaultToken && !tok.IsEmpty()) {
        TF_WARN(
            "Value '%s' is not allowed for flag '%s'; using fallback '%s' "
            "instead",
            tok.GetText(),
            key.GetText(),
            defaultToken.GetText());
    }
    return defaultToken;
}

/// Extracts an absolute path at \p key from \p userArgs, or the empty path if
/// it can't extract.
SdfPath ExtractAbsolutePath(const VtDictionary& userArgs, const TfToken& key)
{
    const std::string s = ExtractString(userArgs, key);
    // Assume that empty strings are empty paths. (This might be an error case.)
    if (s.empty()) {
        return SdfPath();
    }
    // Make all relative paths into absolute paths.
    SdfPath path(s);
    if (path.IsAbsolutePath()) {
        return path;
    } else {
        return SdfPath::AbsoluteRootPath().AppendPath(path);
    }
}

/// Convenience function that takes the result of ExtractVector and converts it to a
/// TfToken::Set.
TfToken::Set ExtractTokenSet(const VtDictionary& userArgs, const TfToken& key)
{
    const std::vector<std::string> vec = ExtractVector<std::string>(userArgs, key);
    TfToken::Set                   result;
    for (const std::string& s : vec) {
        result.insert(TfToken(s));
    }
    return result;
}

// Convert a dictionary entry from std::vector<VtValue> (holding std::string) or
// std::vector<std::string> to ContainerType<ValueType>. ValueType must be instantiable from
// std::string. ContainerType must define insert() method.
template <template <typename...> class ContainerType, typename ValueType>
void StringArrayEntryToStdContainerType(
    VtDictionary&       dict,
    const VtDictionary& guide,
    const std::string&  key)
{
    if (VtDictionaryIsHolding<std::vector<VtValue>>(dict, key)) {
        const auto& vec = dict[key].UncheckedGet<std::vector<pxr::VtValue>>();

        if (std::all_of(vec.begin(), vec.end(), [](const pxr::VtValue& v) {
                return v.IsHolding<std::string>();
            })) {
            ContainerType<ValueType> container;
            for (const auto& val : vec) {
                container.insert(container.end(), ValueType(val.UncheckedGet<std::string>()));
            }
            dict[key] = container;
        }
    } else if (VtDictionaryIsHolding<std::vector<std::string>>(dict, key)) {
        ContainerType<ValueType> container;
        for (const auto& val : VtDictionaryGet<std::vector<std::string>>(dict, key)) {
            container.insert(container.end(), ValueType(val));
        }
        dict[key] = container;
    } else {
        dict[key] = VtDictionaryGet<ContainerType<ValueType>>(guide, key);
        TF_WARN(
            "Expected the value of the key : %s to be a %s",
            key.c_str(),
            typeid(ContainerType<ValueType>).name());
    }
}

// ---------------------------------------------------------------------
// Example of usage:
// guide = {
//      "double": 1.0
//      "TfToken": aTfToken
//      "SdfPath": aSdfPath
//      "boolean": true
//      "failedCoercion": true (bool)
//      "extraKeyNotInDict": "extraValue"
//  }
// dictToCoerce = {
//      "double": 2 (int)
//      "TfToken": "anotherTfToken" (as std::string)
//      "SdfPath": "/anotherSdfPath" (as std::string)
//      "boolean": true (bool)
//      "failedCoercion" : "RandomString" (as std::string)
//  }
//
// After the call to CoerceDictToGuideType(dictToCoerce, guide) the dictToCoerce will be:
// dictToCoerce = {
//      "double": 2.0 (double)
//      "TfToken": anotherTfToken (TfToken)
//      "SdfPath": /anotherSdfPath (SdfPath)
//      "boolean": true (bool)
//      "failedCoercion" : true (bool)
//  }
//
// Iterations over the guide dictionary:
// For the "double" entry, we know how to convert an int to a double, so the value is cast and kept.
// For the "TFToken" entry, we know how to convert a std::string to a TfToken, so the value is cast
// and kept. For the "SdfPath" entry, we know how to convert a std::string to a SdfPath, so the
// value is cast and kept. For the "boolean" entry, the value is already of the correct type, so it
// is kept as is. For the "failedCoercion" entry, the value is not of the correct type, and we don't
// know how to convert it to the correct type, so the value from the guide is put in dictToCoerce
// for this entry. For the "extraKeyNotInDict" entry, it is not in the dictToCoerce, so it is
// skipped.
// ---------------------------------------------------------------------
void CoerceDictToGuideType(VtDictionary& dict, const VtDictionary& guide)
{
    for (const auto& entry : guide) {
        // If the dictionary doesn't have the key, nothing to be done.
        // The goal is not to add new keys to the dictionary, just validate that they are the
        // expected type.
        if (dict.find(entry.first) == dict.end()) {
            continue;
        }
        // Holding the same type, good to go.
        if (dict[entry.first].GetTypeid() == entry.second.GetTypeid()) {
            continue;
        } else if (VtDictionaryIsHolding<std::set<TfToken>>(guide, entry.first)) {
            StringArrayEntryToStdContainerType<std::set, TfToken>(dict, guide, entry.first);
        } else if (VtDictionaryIsHolding<std::set<std::string>>(guide, entry.first)) {
            StringArrayEntryToStdContainerType<std::set, std::string>(dict, guide, entry.first);
        } else if (VtDictionaryIsHolding<std::vector<SdfPath>>(guide, entry.first)) {
            StringArrayEntryToStdContainerType<std::vector, SdfPath>(dict, guide, entry.first);
        } else if (VtDictionaryIsHolding<std::vector<std::string>>(guide, entry.first)) {
            StringArrayEntryToStdContainerType<std::vector, std::string>(dict, guide, entry.first);
        } else if (VtDictionaryIsHolding<std::set<int>>(guide, entry.first)) {
            if (VtDictionaryIsHolding<std::vector<VtValue>>(dict, entry.first)) {
                const auto& vec = dict[entry.first].UncheckedGet<std::vector<pxr::VtValue>>();

                if (std::all_of(vec.begin(), vec.end(), [](const pxr::VtValue& v) {
                        return v.IsHolding<int>();
                    })) {
                    std::set<int> container;
                    for (const auto& val : vec) {
                        container.insert(container.end(), val.UncheckedGet<int>());
                    }
                    dict[entry.first] = container;
                }
            } else {
                dict[entry.first] = VtDictionaryGet<std::set<int>>(guide, entry.first);
                TF_WARN("Expected the value of the key : %s to be a std::set<int>", entry.first);
            }
        } else if (VtDictionaryIsHolding<std::vector<VtDictionary>>(guide, entry.first)) {
            if (VtDictionaryIsHolding<std::vector<VtValue>>(dict, entry.first)) {
                const auto& vec = dict[entry.first].UncheckedGet<std::vector<pxr::VtValue>>();

                if (std::all_of(vec.begin(), vec.end(), [](const pxr::VtValue& v) {
                        return v.IsHolding<VtDictionary>();
                    })) {
                    std::vector<VtDictionary> container;
                    for (const auto& val : vec) {
                        container.insert(container.end(), val.UncheckedGet<VtDictionary>());
                    }
                    dict[entry.first] = container;
                }
            } else {
                dict[entry.first] = VtDictionaryGet<std::vector<VtDictionary>>(guide, entry.first);
                TF_WARN(
                    "Expected the value of the key : %s to be a std::vector<VtDictionary>",
                    entry.first.c_str());
            }
        } else if (VtDictionaryIsHolding<TfToken>(guide, entry.first)) {
            if (VtDictionaryIsHolding<std::string>(dict, entry.first)) {
                dict[entry.first] = TfToken(VtDictionaryGet<std::string>(dict, entry.first));
            } else {
                dict[entry.first] = VtDictionaryGet<TfToken>(guide, entry.first);
                TF_WARN("Expected the value of the key : %s to be a TfToken", entry.first.c_str());
            }
        } else if (VtDictionaryIsHolding<SdfPath>(guide, entry.first)) {
            if (VtDictionaryIsHolding<std::string>(dict, entry.first)) {
                dict[entry.first] = SdfPath(VtDictionaryGet<std::string>(dict, entry.first));
            } else {
                dict[entry.first] = VtDictionaryGet<SdfPath>(guide, entry.first);
                TF_WARN("Expected the value of the key : %s to be a SdfPath", entry.first.c_str());
            }
        } else if (VtDictionaryIsHolding<double>(guide, entry.first)) {
            if (VtDictionaryIsHolding<int>(dict, entry.first)) {
                dict[entry.first] = static_cast<double>(VtDictionaryGet<int>(dict, entry.first));
            } else {
                dict[entry.first] = VtDictionaryGet<double>(guide, entry.first);
                TF_WARN("Expected the value of the key : %s to be a double", entry.first.c_str());
            }
        } else if (VtDictionaryIsHolding<fs::path>(guide, entry.first)) {
            if (VtDictionaryIsHolding<std::string>(dict, entry.first)) {
                dict[entry.first] = fs::path(VtDictionaryGet<std::string>(dict, entry.first));
            } else {
                dict[entry.first] = VtDictionaryGet<std::set<fs::path>>(guide, entry.first);
                TF_WARN(
                    "Expected the value of the key : %s to be a filesystem:path",
                    entry.first.c_str());
            }
        } else if (VtDictionaryIsHolding<AColor>(guide, entry.first)) {
            if (VtDictionaryIsHolding<std::vector<VtValue>>(dict, entry.first)) {
                auto vec = dict[entry.first].UncheckedGet<std::vector<pxr::VtValue>>();
                if (vec.size() == 4
                    && std::all_of(vec.begin(), vec.end(), [](const pxr::VtValue& v) {
                           return v.CanCast<float>();
                       })) {
                    AColor col = AColor(
                        vec[0].Cast<float>().UncheckedGet<float>(),
                        vec[1].Cast<float>().UncheckedGet<float>(),
                        vec[2].Cast<float>().UncheckedGet<float>(),
                        vec[3].Cast<float>().UncheckedGet<float>());
                    dict[entry.first] = col;
                }
            } else {
                dict[entry.first] = VtDictionaryGet<AColor>(guide, entry.first);
                TF_WARN("Expected the value of the key : %s to be a AColor", entry.first.c_str());
            }
        } else if (VtDictionaryIsHolding<std::map<std::string, std::map<std::string, std::string>>>(
                       guide, entry.first)) {
            if (auto holding = VtDictionaryIsHolding<VtDictionary>(dict, entry.first)) {
                VtDictionary d = VtDictionaryGet<VtDictionary>(dict, entry.first);
                std::map<std::string, std::map<std::string, std::string>> result;

                for (const auto& outerPair : d) {
                    if (outerPair.second.IsHolding<VtDictionary>()) {
                        const VtDictionary& innerDict
                            = outerPair.second.UncheckedGet<VtDictionary>();
                        std::map<std::string, std::string> innerMap;

                        for (const auto& innerPair : innerDict) {
                            if (innerPair.second.IsHolding<std::string>()) {
                                innerMap[innerPair.first]
                                    = innerPair.second.UncheckedGet<std::string>();
                            } else {
                                TF_WARN(
                                    "Expected inner dictionary value to be a string for key '%s'",
                                    innerPair.first.c_str());
                            }
                        }
                        result[outerPair.first] = innerMap;
                    } else {
                        TF_WARN(
                            "Expected outer dictionary value to be a dictionary for key '%s'",
                            outerPair.first.c_str());
                    }
                }
                dict[entry.first] = result;
            } else {
                dict[entry.first]
                    = VtDictionaryGet<std::map<std::string, std::map<std::string, std::string>>>(
                        guide, entry.first);
                TF_WARN(
                    "Expected the value of the key : %s to be a std::map<std::string, "
                    "std::map<std::string, std::string>>",
                    entry.first.c_str());
            }
        }

        // Not the same type set the Value from the Guide
        else {
            dict[entry.first] = entry.second;
            TF_WARN("Unsupported type for the value of key : '%s'", entry.first.c_str());
        }
    }
}

void VtDictToJson(const pxr::VtDictionary& dic, QJsonObject& jsonObj)
{
    pxr::VtDictionary validDic(dic);
    std::string       errMsg;

    for (const auto& dicEntry : validDic) {
        auto typeValue = pxr::SdfGetValueTypeNameForValue(dicEntry.second);
        auto type = dicEntry.second.GetTypeName();
        if (dicEntry.second.IsHolding<std::vector<pxr::VtValue>>()) {
            // This convert std::vector<VtValue> and python arrays to VtArray<T>
            // But this also removes some data, like std::vector<T>...
            // The import/export options classes don't use this type, so it's not a problem.
            // But this is very convenient to save the data coming from python context options.
            pxr::SdfConvertToValidMetadataDictionary(&validDic, &errMsg);

            if (!errMsg.empty()) {
                TF_WARN(errMsg);
            }
            break;
        }
    }

    for (const auto& dicEntry : validDic) {
        if (dicEntry.second.IsHolding<bool>()) {
            jsonObj[dicEntry.first.c_str()] = dicEntry.second.Get<bool>();
        } else if (dicEntry.second.IsHolding<int>()) {
            jsonObj[dicEntry.first.c_str()] = dicEntry.second.Get<int>();
        } else if (dicEntry.second.IsHolding<double>()) {
            jsonObj[dicEntry.first.c_str()] = dicEntry.second.Get<double>();
        } else if (dicEntry.second.IsHolding<float>()) {
            jsonObj[dicEntry.first.c_str()] = dicEntry.second.Get<float>();
        } else if (dicEntry.second.IsHolding<std::string>()) {
            jsonObj[dicEntry.first.c_str()] = dicEntry.second.Get<std::string>().c_str();
        } else if (dicEntry.second.IsHolding<pxr::TfToken>()) {
            jsonObj[dicEntry.first.c_str()] = dicEntry.second.Get<pxr::TfToken>().GetText();
        } else if (dicEntry.second.IsHolding<pxr::SdfPath>()) {
            jsonObj[dicEntry.first.c_str()]
                = dicEntry.second.Get<pxr::SdfPath>().GetString().c_str();
        } else if (dicEntry.second.IsHolding<fs::path>()) {
            jsonObj[dicEntry.first.c_str()] = dicEntry.second.Get<fs::path>().string().c_str();
        } else if (dicEntry.second.IsHolding<AColor>()) {
            auto       color = dicEntry.second.Get<AColor>();
            QJsonArray colorArray;
            colorArray << color.r << color.g << color.b << color.a;
            jsonObj[dicEntry.first.c_str()] = colorArray;
        } else if (dicEntry.second.IsArrayValued()) {
            QJsonArray array;
            auto       typeValue = pxr::SdfGetValueTypeNameForValue(dicEntry.second);

            if (typeValue == "string[]") {
                for (const auto& item : dicEntry.second.Get<pxr::VtArray<std::string>>()) {
                    array << item.c_str();
                }
            } else if (typeValue == "int[]") {
                for (const auto& item : dicEntry.second.Get<pxr::VtArray<int>>()) {
                    array << item;
                }
            } else if (typeValue == "float[]") {
                for (const auto& item : dicEntry.second.Get<pxr::VtArray<float>>()) {
                    array << item;
                }
            } else if (typeValue == "double[]") {
                for (const auto& item : dicEntry.second.Get<pxr::VtArray<double>>()) {
                    array << item;
                }
            } else if (typeValue == "bool[]") {
                for (const auto& item : dicEntry.second.Get<pxr::VtArray<bool>>()) {
                    array << item;
                }
            } else if (typeValue == "TfToken[]") {
                for (const auto& item : dicEntry.second.Get<pxr::VtArray<pxr::TfToken>>()) {
                    array << item.GetText();
                }
            } else if (typeValue == "SdfPath[]") {
                for (const auto& item : dicEntry.second.Get<pxr::VtArray<pxr::SdfPath>>()) {
                    array << item.GetString().c_str();
                }
            }
            jsonObj[dicEntry.first.c_str()] = array;
        } else if (dicEntry.second.IsHolding<std::vector<int>>()) {
            QJsonArray array;
            for (const auto& item : dicEntry.second.UncheckedGet<std::vector<int>>()) {
                array << item;
            }
            jsonObj[dicEntry.first.c_str()] = array;
        } else if (dicEntry.second.IsHolding<std::vector<float>>()) {
            QJsonArray array;
            for (const auto& item : dicEntry.second.UncheckedGet<std::vector<float>>()) {
                array << item;
            }
            jsonObj[dicEntry.first.c_str()] = array;
        } else if (dicEntry.second.IsHolding<std::vector<double>>()) {
            QJsonArray array;
            for (const auto& item : dicEntry.second.UncheckedGet<std::vector<double>>()) {
                array << item;
            }
            jsonObj[dicEntry.first.c_str()] = array;
        } else if (dicEntry.second.IsHolding<std::vector<std::string>>()) {
            QJsonArray array;
            for (const auto& item : dicEntry.second.UncheckedGet<std::vector<std::string>>()) {
                array << item.c_str();
            }
            jsonObj[dicEntry.first.c_str()] = array;
        } else if (dicEntry.second.IsHolding<std::vector<pxr::TfToken>>()) {
            QJsonArray array;
            for (const auto& item : dicEntry.second.UncheckedGet<std::vector<pxr::TfToken>>()) {
                array << item.GetText();
            }
            jsonObj[dicEntry.first.c_str()] = array;
        } else if (dicEntry.second.IsHolding<std::vector<pxr::SdfPath>>()) {
            QJsonArray array;
            for (const auto& item : dicEntry.second.UncheckedGet<std::vector<pxr::SdfPath>>()) {
                array << item.GetString().c_str();
            }
            jsonObj[dicEntry.first.c_str()] = array;
        } else if (dicEntry.second.IsHolding<std::set<std::string>>()) {
            QJsonArray array;
            for (const auto& item : dicEntry.second.UncheckedGet<std::set<std::string>>()) {
                array << item.c_str();
            }
            jsonObj[dicEntry.first.c_str()] = array;
        } else if (dicEntry.second.IsHolding<std::set<pxr::TfToken>>()) {
            QJsonArray array;
            for (const auto& item : dicEntry.second.UncheckedGet<std::set<pxr::TfToken>>()) {
                array << item.GetString().c_str();
            }
            jsonObj[dicEntry.first.c_str()] = array;
        } else if (dicEntry.second.IsHolding<std::set<int>>()) {
            QJsonArray array;
            for (const auto& item : dicEntry.second.UncheckedGet<std::set<int>>()) {
                array << item;
            }
            jsonObj[dicEntry.first.c_str()] = array;
        } else if (dicEntry.second
                       .IsHolding<std::map<std::string, std::map<std::string, std::string>>>()) {
            QJsonObject map;
            const auto& mapValue
                = dicEntry.second.Get<std::map<std::string, std::map<std::string, std::string>>>();
            for (const auto& outerKey : mapValue) {
                QJsonObject innerMap;
                for (const auto& innerKey : outerKey.second) {
                    innerMap[innerKey.first.c_str()] = innerKey.second.c_str();
                }
                map[outerKey.first.c_str()] = innerMap;
            }
            jsonObj[dicEntry.first.c_str()] = map;
        } else if (dicEntry.second.IsHolding<std::vector<pxr::VtDictionary>>()) {
            QJsonArray array;
            for (const auto& innerDict :
                 dicEntry.second.UncheckedGet<std::vector<pxr::VtDictionary>>()) {
                QJsonObject subDictJson;
                VtDictToJson(innerDict, subDictJson);
                array.push_back(subDictJson);
            }
            jsonObj[dicEntry.first.c_str()] = array;
        } else if (dicEntry.second.IsHolding<pxr::VtDictionary>()) {
            QJsonObject subDict;
            VtDictToJson(dicEntry.second.Get<pxr::VtDictionary>(), subDict);
            jsonObj[dicEntry.first.c_str()] = subDict;
        }

        else {
            TF_WARN("Failed to serialize key : %s", dicEntry.first);
        }
    }
}

void VtDictFromString(const std::string& stdStr, pxr::VtDictionary& dict)
{
    pxr::JsParseError error;
    pxr::JsValue      jsDict = pxr::JsParseString(stdStr, &error);

    if (jsDict.IsNull()) {
        if (!error.reason.empty()) {
            TF_WARN(
                "Failed to extract dictionary from input (line %d, col %d): %s",
                error.line,
                error.column,
                error.reason.c_str());
        }
    }

    const VtValue vtdict
        = JsValueTypeConverter<VtValue, VtDictionary, /*UseInt64*/ false>::Convert(jsDict);

    dict = vtdict.IsHolding<VtDictionary>() ? vtdict.UncheckedGet<VtDictionary>() : VtDictionary();
}

} // namespace DictUtils

} // namespace MAXUSD_NS_DEF