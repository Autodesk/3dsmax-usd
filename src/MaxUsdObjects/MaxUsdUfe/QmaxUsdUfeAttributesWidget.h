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
#pragma once
#include <pxr/base/tf/type.h>
#include <pxr/usd/usd/prim.h>

#include <ufe/selection.h>

#include <MaxUsd.h>
#include <QtWidgets/qwidget.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

class QmaxUsdUfeAttributesWidgetPrivate;
class QmaxUsdUfeAttributesWidget : public QWidget
{
    Q_OBJECT

public:
    QmaxUsdUfeAttributesWidget();
    ~QmaxUsdUfeAttributesWidget() override;

    /** Create a new QmaxUsdUfeAttributesWidget containing controls for the
     * supported attributes of the USD schema definition of the given type, or a
     * nullptr, if no supported attribute is present.
     * \param selection The UFE selection to create the widget for.
     * \param type The USD schema type to create the widget for.
     * \param handledAttributeNames A set of attribute names. The functions will
     *        insert all the names of attributes that are handled by the widget.
     * \return A new QmaxUsdUfeAttributesWidget, or nullptr (of no supported
     * attributes were found as part of this type). The returned widget is
     * already bound to the the UFE attributes of the individual objects in the
     * given selection.
     * The object name of the widget can be used as a rollup title. */
    static std::unique_ptr<QmaxUsdUfeAttributesWidget> create(
        const Ufe::Selection&  selection,
        const pxr::TfType&     type,
        std::set<std::string>& handledAttributeNames);

    /** Create a new QmaxUsdUfeAttributesWidget containing controls for the
     * attributes passed in, or nullptr, if no supported attributes are given.
     * \param selection The UFE selection to create the widget for.
     * \param attributeNames The names of the attributes to create the widget for.
     * \param handledAttributeNames A set of attribute names. The functions will
     *        insert all the names of attributes that are handled by the widget.
     * \return A new QmaxUsdUfeAttributesWidget, or nullptr (of no supported
     * attributes were found as part of this type). The returned widget is
     * already bound to the the UFE attributes of the individual objects in the
     * given selection. */
    static std::unique_ptr<QmaxUsdUfeAttributesWidget> create(
        const Ufe::Selection&           selection,
        const std::vector<std::string>& attributeNames,
        std::set<std::string>&          handledAttributeNames);

    /** Create a new QmaxUsdUfeAttributesWidget containing some common controls
     * as well as controls for the supported attributes that are not already
     * handled by other USD Schema definitions.
     * \param selection The UFE selection to create the widget for.
     * \param handledAttributeNames A set of attribute names that already is
     *        handled by other rollups.
     * \return A new QmaxUsdUfeAttributesWidget. The returned widget is already
     * bound to the the UFE attributes of the individual objects in the given
     * selection.
     * The object name of the widget can be used as a rollup title. */
    static std::unique_ptr<QmaxUsdUfeAttributesWidget> createMetaData(
        const Ufe::Selection&        selection,
        const std::set<std::string>& handledAttributeNames);

private:
    const std::unique_ptr<QmaxUsdUfeAttributesWidgetPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QmaxUsdUfeAttributesWidget);
};

} // namespace ufe
} // namespace MAXUSD_NS_DEF
