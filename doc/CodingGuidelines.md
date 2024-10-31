
This document outlines coding guidelines for contributions to the  [3dsmax-usd](https://github.com/autodesk/3dsmax-usd) project.

# C++ Coding Guidelines

Many of the C++ coding guidelines below are validated and enforced through the use of `clang-format` which is provided by the [LLVM project](https://github.com/llvm/llvm-project). Since the adjustments made by `clang-format` can vary from version to version, we standardize this project on a single `clang-format` version to ensure consistent results for all contributions made to 3dsmax-usd.

|                      | Version | Source Code | Release |
|:--------------------:|:-------:|:-----------:|:-------:|
|  `clang-format`/LLVM |  10.0.0 | [llvmorg-10.0.0 Tag](https://github.com/llvm/llvm-project/tree/llvmorg-10.0.0) | [LLVM 10.0.0](https://github.com/llvm/llvm-project/releases/tag/llvmorg-10.0.0) |

## Foundation/Critical
### License notice
Every file should start with the Apache 2.0 licensing statement:
```cpp
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an “AS IS” BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
```

### Copyright notice
* Every file should contain at least one Copyright line at the top, which can be to Autodesk or the individual/company that contributed the code.
* Multiple copyright lines are allowed, and if a significant new contribution is made to an existing file then an individual or company can append a new line to the copyright section.
* The year the original contribution is made should be included but there is no requirement to update this every year.
* There is no requirement that an Autodesk copyright line should be in all files. If an individual or company contributes new files they do not have to add both their name and Autodesk's name.
* If existing code is being refactored or moved within the repo then the original copyright lines should be maintained. You should only append a new copyright line if significant new changes are added. For example, if some utility code is being moved from a plugin into a common area, and the work involved is only minor name changes or include path updates, then the original copyright should be maintained. In this case, there is no requirement to add a new copyright for the person that handled the refactoring.

### #pragma once vs include guard
The project makes use of `#pragma once` to avoid files being `#include’d` multiple times (which can cause duplication of definitions & compilation errors). While it is a non-standard and non-portable extension, it is widely supported. It is also a choice aligned with the 3ds Max SDK. The platform and compiler used (Windows/VS) fully support the preprocessor option.

### Naming (file, type, variable, constant, function, namespace, macro, template parameters, schema names)
**General Naming Rules**
MaxUsd strives to use “camel case” naming. That is, each word is capitalized, except possibly the first word:
* UpperCamelCase
* lowerCamelCase

While underscores in names (_) (e.g., as separator) are not strictly forbidden, they are strongly discouraged.
Optimize for readability by selecting names that are clear to others (e.g., people on different teams.)
Use names that describe the purpose or intent of the object. Do not worry about saving horizontal space. It is more important to make your code easily understandable by others. Minimize the use of abbreviations that would likely be unknown to someone outside your project (especially acronyms and initialisms).

**File Names**
Filenames should be UpperCamelCase and should not include underscores (_) or dashes (-).
C++ files should end in .cpp and header files should end in .h.
In general, make your filenames as specific as possible. For example:
```
StageData.cpp
StageData.h
```

**Type Names**
All type names (i.e., classes, structs, type aliases, enums, and type template parameters) should use UpperCamelCase, with no underscores. For example:
```cpp
class MaxUsdStageData;
class ImportData;
enum Roles;
```

**Variable Names**
**Class/Struct Data Members**
**Constant Names**
All variables, data member and constant names, including function parameters and data members should use lowerCamelCase, with no underscores. For example:
```cpp
const INode* parentNode;
const MSTR& rayDirection
bool* drawRenderPurpose;
```

**Function/Method Names**
All functions should be UpperCamelCase. This avoids inconsistencies with the 3ds Max SDK. For example:
```cpp
MSTR Name() const override;
void RegisterExitCallback();
```

**Namespace Names**
Namespace names should be UpperCamelCase. Top-level namespace names are based on the project name.
```cpp
namespace MaxUsd {}
```

**Enumerator Names**
Enumerators (for both scoped and unscoped enums) should be UpperCamelCase.

The enumeration name, `StringPolicy` is a type and therefore mixed case.
```cpp
enum class StringPolicy
{
  StringOptional,
  StringMustHaveValue
};
```

**Macro Names**
In general, macros should be avoided (see [Modern C++](https://docs.google.com/document/d/1Jvbpfh2WNzHxGQtjqctZ1K1lnpaAtHOUwm0kmmEcxjY/edit#heading=h.ynbggnv41p3) ). However, if they are absolutely needed, macros should be all capitals, with words separated by underscores.
```cpp
#define ROUND(x) …
#define PI_ROUNDED 3.0
```

### Documentation (class, method, variable, comments)
* [Doxygen ](http://www.doxygen.nl/index.html) will be used to generate documentation from MaxUsd C++ sources.
* Doxygen tags must be used to document classes, function parameters, function return values, and thrown exceptions.
* The MaxUsd project does require the use of any Doxygen formatting style ( [Doxygen built-in formatting](http://www.doxygen.nl/manual/commands.html) )
* Comments for users of classes and functions must be written in headers files. Comments in definition files are meant for contributors and maintainers.

### Namespaces

#### In header files (e.g. .h)

* **Required:** to use fully qualified namespace names. Global scope using directives are not allowed.  Inline code can use using directives in implementations, within a scope, when there is no other choice (e.g. when using macros, which are not namespaced).

```cpp
// In aFile.h
inline PXR_NS::UsdPrim prim() const
{
    PXR_NAMESPACE_USING_DIRECTIVE
    TF_VERIFY(item != nullptr);
    return item->prim();
}
```

#### In implementation files (e.g. .cpp)

* **Recommended:** to use fully qualified namespace names, unless clarity or readability is degraded by use of explicit namespaces, in which case a using directive is acceptable.
* **Recommended:** to use the existing namespace style, and not make gratuitous changes. If the file is using explicit namespaces, new code should follow this style, unless the changes are so significant that clarity or readability is degraded.  If the file has one or more using directives, new code should follow this style.

### Include directive
For source files (.cpp) with an associated header file (.h) that resides in the same directory, it should be `#include`'d with double quotes and no path.  This formatting should be followed regardless of with whether the associated header is public or private. For example:
```cpp
// In foobar.cpp
#include "foobar.h"
```

All included public header files from outside and inside the project should be `#include`’d using angle brackets. For example:
```cpp
#include <pxr/base/tf/stringUtils.h>
#include <MaxUsd/MeshConversion/MeshConverter.h>
```

Private project’s header files should be `#include`'d using double quotes, and a relative path. Private headers may live in the same directory or sub-directories, but they should never be included using "._" or ".._" as part of a relative path. For example:
```cpp
#include "privateUtils.h"
#include "pvt/helperFunctions.h"
```

### Include order
Headers should be included in the following order, with each section separated by a blank line and files sorted alphabetically:

1. Related header
2. All private headers
3. All public headers from this repository (3dsmax-usd)
4. UsdUfe library headers
5. Pixar + USD headers
6. Autodesk + 3ds Max headers
7. Other libraries' headers
8. C++ standard library headers
9. C system headers
10. Conditional includes

```cpp
#include "MaxSceneBuilder.h"

#include "private/util.h"

#include <MaxUsd/Translators/PrimReaderRegistry.h>
#include <MaxUsd/Translators/TranslatorXformable.h>

#include <usdUfe/ufe/UsdSceneItem.h>

#include <pxr/base/tf/pyPolymorphic.h>

#include <GetCOREInterface.h>
#include <maxscript/maxscript.h>

#include <string>

#ifdef IS_MAX2023_OR_GREATER
#include <Graphics/UpdateDisplayContext.h>
#include <Graphics/InstanceDisplayGeometry.h>
#endif
```

### Conditional compilation (3ds Max, USD, UFE version)
**3ds Max**
* A series of `IS_MAX20XX_OR_GREATER` defines are available (see `MaxUsd/Utilities/MaxSupportUtils.h`)
* `MAX_RELEASE` is the consistent macro to test 3ds Max version (see `maxsdk/include/plugapi.h` from the 3ds Max SDK)

**UFE**
* Each version of Ufe contains a features available define (in ufe.h) such as `UFE_V4_FEATURES_AVAILABLE` that can be used for conditional compilation on code depending on Ufe Version.

**USD**
* `PXR_VERSION` is the macro to test USD version (`PXR_MAJOR_VERSION` * 10000 + `PXR_MINOR_VERSION` * 100 + `PXR_PATCH_VERSION`)

Respect the minimum supported version for 3ds Max and USD stated in [build.md](https://github.com/Autodesk/3dsmax-usd/dev/doc/build.md) .

### std over boost
Recent extensions to the C++ standard introduce many features previously only found in [boost](http://boost.org). To avoid introducing additional dependencies, developers should strive to use functionality in the C++ std over boost. If you encounter usage of boost in the code, consider converting this to the equivalent std mechanism. 
Our library currently has the following boost dependencies:
* `boost::python`

## Modern C++
Our goal is to develop [3dsmax-usd](https://github.com/autodesk/3dsmax-usd) following modern C++ practices. We’ll follow the [C++ Core Guidelines](http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) and pay attention to:
* `using` (vs `typedef`) keyword
* `virtual`, `override` and `final` keyword
* `default` and `delete` keywords
* `auto` keyword
* initialization - `{}`
* `nullptr` keyword
* …

## Diagnostic Facilities

Developers are encouraged to use TF library diagnostic facilities in 3ds Max USD. Please follow below guideline for picking the correct facility: https://graphics.pixar.com/usd/docs/api/page_tf__diagnostic.html

In MaxUsd, `DiagnosticDelegate` converts TF diagnostics into native 3ds Max infos, warnings, and errors. The environment flag `MAXUSD_SHOW_FULL_DIAGNOSTICS`, controls the granularity of TF error/warning/status messages being displayed in 3ds Max:

e.g
```
[2024-05-23 12:06:53.623] [USDImport] [warning] Unexpected input type or mapped value - normal3f:(0, 0, 1):normal
```
vs
```
[2024-05-23 12:06:53.623] [USDImport] [warning] Unexpected input type or mapped value - normal3f:(0, 0, 1):normal -- Warning in usd_material_reader.output_max_material at line 463 of d:\usd-component-2024/contents/scripts//materials\usd_material_reader.py
```

# Coding guidelines for Python
We are adopting the [PEP-8](https://www.python.org/dev/peps/pep-0008) style for Python Code with the following modification:
* Mixed-case for variable and function names are allowed 

[Pylint](https://www.pylint.org/) is recommended for automation.
