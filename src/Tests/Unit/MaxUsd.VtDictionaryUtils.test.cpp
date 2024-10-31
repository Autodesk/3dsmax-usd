//
// Copyright 2024 Autodesk
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
#include <gtest/gtest.h>

#include "MaxUsd/Utilities/VtDictionaryUtils.h"

using namespace MaxUsd;

namespace {
const std::string doubleStr = "double";
const std::string stringStr = "string";
const std::string TfTokenStr = "TfToken";
const std::string SdfPathStr = "SdfPath";
const std::string vectorSdfPathStr = "vectorSdfPath";
const std::string setStringStr = "setString";

}

TEST(VtDictionaryUtilsTest, TestCoerceDictToGuideType)
{
    // Create a guide dictionary with some values
    pxr::VtDictionary guideDict;
    guideDict[doubleStr] = 1.0;
    guideDict[stringStr] = std::string(stringStr);
    guideDict[TfTokenStr] = pxr::TfToken(TfTokenStr);
    guideDict[SdfPathStr] = pxr::SdfPath(SdfPathStr);
    guideDict[vectorSdfPathStr] = std::vector<pxr::SdfPath>{ pxr::SdfPath(vectorSdfPathStr) };
    guideDict[setStringStr] = std::set<std::string>{ setStringStr };

    // Create the dictionary to be coerced
    pxr::VtDictionary coercedDict;
    // Int assigned to test the coercing of int to double
    coercedDict[doubleStr] = 2;
    // Leave this one out, and make sure later that it's not added to the dict.
    //coercedDict[stringStr] = stringStr;
    // Test string -> TfToken
    coercedDict[TfTokenStr] = TfTokenStr;
    // Test string -> SdfPath
    coercedDict[SdfPathStr] = SdfPathStr;
    // Test vector<string> -> vector<SdfPath>
    coercedDict[vectorSdfPathStr] = std::vector<std::string>{ vectorSdfPathStr };
    // Test vector<string> -> set<string>
    coercedDict[setStringStr] = std::vector<std::string>{ setStringStr };

    DictUtils::CoerceDictToGuideType(coercedDict, guideDict);

    EXPECT_FALSE(pxr::VtDictionaryIsHolding<std::string>(coercedDict, stringStr));
    EXPECT_DOUBLE_EQ(coercedDict[doubleStr].Get<double>(), 2.0);
    EXPECT_EQ(coercedDict[TfTokenStr].Get<pxr::TfToken>(), guideDict[TfTokenStr].Get<pxr::TfToken>());
    EXPECT_EQ(coercedDict[SdfPathStr].Get<pxr::SdfPath>(), guideDict[SdfPathStr].Get<pxr::SdfPath>());
    EXPECT_EQ(coercedDict[vectorSdfPathStr].Get<std::vector<pxr::SdfPath>>(),
            guideDict[vectorSdfPathStr].Get<std::vector<pxr::SdfPath>>());
    EXPECT_EQ(coercedDict[setStringStr].Get<std::set<std::string>>(),
            guideDict[setStringStr].Get<std::set<std::string>>());
}