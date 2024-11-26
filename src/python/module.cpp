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
#include <pxr/base/tf/pyModule.h>
#include <pxr/pxr.h>

#include <boost/python/docstring_options.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

TF_WRAP_MODULE
{
    // This will enable user-defined docstrings and python signatures,
    // while disabling the C++ signatures
    boost::python::docstring_options local_docstring_options(true, true, false);

    TF_WRAP(MaxSceneBuilderOptions);
    TF_WRAP(UsdSceneBuilderOptions);
    TF_WRAP(ReadJobContext);
    TF_WRAP(PrimReader);
    TF_WRAP(PrimWriter);
    TF_WRAP(TranslationUtils);
    TF_WRAP(TranslatorMaterial);
    TF_WRAP(ShaderReader);
    TF_WRAP(ShaderWriter);
    TF_WRAP(ShadingMode);
    TF_WRAP(ExportChaserRegistryFactoryContext);
    TF_WRAP(ImportChaserRegistryFactoryContext);
    TF_WRAP(ExportChaser);
    TF_WRAP(ImportChaser);
    TF_WRAP(JobContextRegistry);
    TF_WRAP(MeshConverter);
    TF_WRAP(MaterialConverter);
    TF_WRAP(Utilities);
    TF_WRAP(ExportTime);
    TF_WRAP(Interval);
}
