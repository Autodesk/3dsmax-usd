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
#include "QmaxUsdUfeAttributesWidget.h"

#include "MaxUsdEditCommand.h"
#include "UfeUtils.h"

#include <UFEUI/Widgets/qFilenameEdit.h>
#include <UFEUI/genericCommand.h>
#include <UFEUI/utils.h>

#include <MaxUsd/Utilities/UiUtils.h>
#include <MaxUsd/Widgets/ElidedLabel.h>

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <pxr/usd/kind/registry.h>
#include <pxr/usd/sdf/schema.h>
#include <pxr/usd/usd/modelAPI.h>

#include <Qt/QmaxMainWindow.h>
#include <Qt/QmaxMultiSpinner.h>
#include <Qt/QmaxSpinBox.h>
#include <ufe/attribute.h>
#include <ufe/attributes.h>
#include <ufe/attributesNotification.h>
#include <ufe/notification.h>
#include <ufe/scene.h>
#include <ufe/sceneNotification.h>
#include <ufe/undoableCommandMgr.h>

#include <QtWidgets/QtWidgets>
#include <maxapi.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

inline std::vector<std::string> extractSchemaAttributeNames(const TfType& t)
{
    static std::map<TfType, std::vector<std::string>> cache;
    auto                                              it = cache.find(t);
    if (it != cache.end()) {
        return it->second;
    }

    // Extract attribute names via Python. Currently there seems to exist no way
    // to extract those using purely C++.

    std::vector<std::string> results;

    PyGILState_STATE gstate = PyGILState_Ensure();
    {
        auto pw = t.GetPythonClass();
        if (pw) {
            auto      names = pw.attr("GetSchemaAttributeNames")(false);
            const int len = boost::python::len(names);
            for (int i = 0; i < len; i++) {
                auto name = names.attr("__getitem__")(i);
                if (name.is_none()) {
                    continue;
                }
                results.emplace_back(boost::python::extract<std::string>(name));
            }
        }
    }
    PyGILState_Release(gstate);

    cache[t] = results;
    return results;
}

// Return a section name from the input schema type name. This section name is a
// pretty name used in the UI.
inline QString rollupTitleFromTypeName(const std::string& typeName)
{
    QString result = QString::fromStdString(typeName);

    // List of special rules for adjusting the base schema names.
    const std::vector<std::pair<QString, QString>> prefixesToAdjust {
        { "UsdAbc", "" },
        { "UsdGeomGprim", "GeometricPrim" },
        { "UsdGeomImageable", "Display" },
        { "UsdGeom", "" },
        { "UsdHydra", "" },
        { "UsdImagingGL", "" },
        { "UsdLux", "" },
        { "UsdMedia", "" },
        { "UsdRender", "" },
        { "UsdRi", "" },
        { "UsdShade", "" },
        { "UsdSkelAnimation", "SkelAnimation" },
        { "UsdSkelBlendShape", "BlendShape" },
        { "UsdSkelSkeleton", "Skeleton" },
        { "UsdSkelRoot", "SkelRoot" },
        { "UsdUI", "" },
        { "UsdUtils", "" },
        { "UsdVol", "" }
    };

    for (const auto& it : prefixesToAdjust) {
        if (result.startsWith(it.first)) {
            result.replace(0, it.first.length(), it.second);
            break;
        }
    }

    result = QString::fromStdString(MaxUsd::Ui::PrettifyName(result.toStdString()));

    // if the schema name ends with "api" or "API", trim it.
    if (result.endsWith("api") || result.endsWith("API")) {
        result.chop(3);
    }

    return result;
}

template <typename T>
std::vector<std::shared_ptr<T>> castAttributes(const std::vector<Ufe::Attribute::Ptr>& attributes)
{
    std::vector<std::shared_ptr<T>> castedAttributes;
    for (const auto& a : attributes) {
        auto casted = std::dynamic_pointer_cast<T>(a);
        if (!casted) {
            return {};
        }
        castedAttributes.emplace_back(casted);
    }
    return castedAttributes;
}

template <typename T, typename V> std::unique_ptr<T> commonValue(const std::vector<V>& attributes)
{
    std::unique_ptr<T> value;
    for (const auto& a : attributes) {
        if (!value) {
            value = std::make_unique<T>(a->get());
        } else if (*value != a->get()) {
            value.reset();
            break;
        }
    }
    return value;
}

template <typename T>
std::unique_ptr<T>
commonValue(const std::vector<std::shared_ptr<Ufe::TypedAttribute<T>>>& attributes)
{
    return commonValue<T, std::shared_ptr<Ufe::TypedAttribute<T>>>(attributes);
}

std::unique_ptr<std::string> commonValue(const std::vector<Ufe::AttributeFilename::Ptr>& attributes)
{
    return commonValue<std::string, Ufe::AttributeFilename::Ptr>(attributes);
}

std::unique_ptr<std::string>
commonValue(const std::vector<Ufe::AttributeEnumString::Ptr>& attributes)
{
    return commonValue<std::string, Ufe::AttributeEnumString::Ptr>(attributes);
}

/**
 * \brief RAII class meant for wrapping attribute edits with other commands. Currently
 * used to work around USD refresh issues.
 * \tparam A The attribute type.
 */
template <typename A> class AttrSetWrapper
{
public:
    AttrSetWrapper(
        std::shared_ptr<Ufe::CompositeUndoableCommand> compositeCmd,
        const std::vector<std::shared_ptr<A>>&         attributes,
        const std::string&                             attributeName)
        : compositeCmd(compositeCmd)
        , attributes(attributes)
        , attributeName(attributeName)
    {
        this->compositeCmd = compositeCmd;

        // Working around hdStorm issue where building model cards "live" can cause
        // a hang when setting card textures. The idea is to temporarily disable the
        // draw mode by setting an empty model kind. Once we are done, we reapply the model kind.
        // See https://github.com/PixarAnimationStudios/OpenUSD/issues/3239
        // There are also general refresh issues in USD when editing the model api, so do this trick
        // for all attributes part of the schema.

        if (attributeName.rfind("model:", 0) != std::string::npos) {
            for (auto a : attributes) {
                auto item = a->sceneItem();
                auto prim = MaxUsd::ufe::ufePathToPrim(item->path());

                pxr::TfToken kindBefore;
                pxr::UsdModelAPI { prim }.GetKind(&kindBefore);
                kindsBefore.push_back(kindBefore);

                pxr::TfToken newKind;
                auto cmdLambda = [prim, newKind, kindBefore](UfeUI::GenericCommand::Mode mode) {
                    pxr::UsdModelAPI { prim }.SetKind(
                        mode == UfeUI::GenericCommand::Mode::kRedo ? newKind : kindBefore);
                };
                auto genericCmd = UfeUI::GenericCommand::create(cmdLambda, "");
                compositeCmd->append(genericCmd);
            }
        }
    }

    ~AttrSetWrapper()
    {
        if (!kindsBefore.empty()) {
            for (int i = 0; i < attributes.size(); ++i) {
                auto item = attributes[i]->sceneItem();
                auto prim = MaxUsd::ufe::ufePathToPrim(item->path());

                pxr::TfToken kindBefore;
                pxr::TfToken newKind = kindsBefore[i];

                auto cmdLambda = [prim, newKind, kindBefore](UfeUI::GenericCommand::Mode mode) {
                    pxr::UsdModelAPI { prim }.SetKind(
                        mode == UfeUI::GenericCommand::Mode::kRedo ? newKind : kindBefore);
                };
                auto genericCmd = UfeUI::GenericCommand::create(cmdLambda, "");
                compositeCmd->append(genericCmd);
            }
        }
    }

private:
    std::shared_ptr<Ufe::CompositeUndoableCommand> compositeCmd;
    const std::vector<std::shared_ptr<A>>&         attributes;
    const std::string&                             attributeName;

    std::vector<pxr::TfToken> kindsBefore;
};

template <typename A, typename T>
void applyChanges(
    const Ufe::Path&                       itemPath,
    const std::vector<std::shared_ptr<A>>& attributes,
    const std::string&                     attributeName,
    const T&                               value)
{
    const auto compositeCmd = Ufe::CompositeUndoableCommand::create({});

    {
        // RAII command wrapper working around some USD refresh issues...
        const auto wrap = AttrSetWrapper<A> { compositeCmd, attributes, attributeName };

        for (const auto& a : attributes) {
            compositeCmd->append(a->setCmd(value));
        }
    }

    if (!compositeCmd->cmdsList().empty()) {
        auto commandName = QApplication::translate("USDStageObject", "Change USD attribute '%1'")
                               .arg(QString::fromStdString(attributeName))
                               .toStdString();
        Ufe::UndoableCommandMgr::instance().executeCmd(
            UfeUi::EditCommand::create(itemPath, compositeCmd, commandName));
    }
}

void applyChanges(
    const Ufe::Path&                       itemPath,
    const UfeUI::GenericCommand::Callback& callback,
    const std::string&                     commandName)
{
    auto genericCmd = UfeUI::GenericCommand::create(callback, commandName);
    Ufe::UndoableCommandMgr::instance().executeCmd(
        UfeUi::EditCommand::create(itemPath, genericCmd, commandName));
}

void applyChanges(
    const Ufe::Path&                  itemPath,
    UfeUI::GenericCommand::Callback&& callback,
    const std::string&                commandName)
{
    auto genericCmd = UfeUI::GenericCommand::create(callback, commandName);
    Ufe::UndoableCommandMgr::instance().executeCmd(
        UfeUi::EditCommand::create(itemPath, genericCmd, commandName));
}

QString cleanDocumentation(const std::string& doc)
{
    static std::unordered_map<std::string, QString> cache;
    auto                                            cacheIt = cache.find(doc);
    if (cacheIt != cache.end()) {
        return cacheIt->second;
    }

    QString result;
    result.reserve(static_cast<int>(doc.size()));

    auto qdoc = QString::fromStdString(doc);

    // -- remove the markdown lists and convert them into regular blocks.
    QRegularExpression listRegexp(
        R"((?>(\n\n)|^)(^((?<indent>\s+)- )(?<firstLine>.+)\n(?<nextLines>(\g{indent}  (.+)\n?)*)))",
        QRegularExpression::MultilineOption);
    auto listMatch = listRegexp.globalMatch(qdoc);
    auto replacements = std::vector<std::tuple<int, int, QString>>();
    while (listMatch.hasNext()) {
        auto m = listMatch.next();
        auto l = m.captured("firstLine") + "\n" + m.captured("nextLines").simplified();
        replacements.push_back(std::make_tuple(m.capturedStart(), m.capturedLength(), l));
    }
    bool isLast = true;
    for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
        auto    start = std::get<0>(*it);
        auto    len = std::get<1>(*it);
        QString s = std::get<2>(*it);
        qdoc.replace(start, len, QString("\n\n") + s + (isLast ? "\n\n" : ""));
        isLast = false;
    }

    auto lines = qdoc.split("\n");
    for (const auto& line : lines) {
        auto l = line.simplified(); // replaces continuous white spaces, trims begin and end
        if (!result.isEmpty()) {
            if (l.isEmpty()) {
                if (result.back() != '\n') {
                    result += "\n\n";
                }
                continue;
            }
            if (result.back() != '\n') {
                result += " ";
            }
        }
        result += l;
    }

    { // converts the markdown bold/emphasized "__something__" or "_something_" to just "something"
        QRegularExpression bold(
            R"((?>^|\s)(?<open>(__)|(_))(?U:.+)(?<close>(\g{open}))(?>[:.,;\s]))",
            QRegularExpression::MultilineOption);
        auto boldMatch = bold.globalMatch(result);
        replacements.clear();
        while (boldMatch.hasNext()) {
            auto match = boldMatch.next();
            replacements.emplace_back(
                match.capturedStart("open"), match.capturedLength("open"), "");
            replacements.emplace_back(
                match.capturedStart("close"), match.capturedLength("close"), "");
        }
        for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
            auto start = std::get<0>(*it);
            auto len = std::get<1>(*it);
            result.replace(start, len, "");
        }
    }

    { // make a new line after the first sentence
        QRegularExpression re(R"((?<!i\.e)\.(\s+|\n+))");
        auto               match = re.match(result);
        if (match.hasMatch()) {
            result.replace(match.capturedStart(), match.capturedLength(), ".\n\n");
        }
    }

    { // searches see /sa links with or without html style hyper links
        QRegularExpression sa(
            R"((^|(see)?\s)((\\sa)|(\\see))\s+((<a.*<\/a>)|(\S+))\.?)",
            QRegularExpression::MultilineOption);
        result.replace(sa, "");
    }

    { // replaces markdown URLs with their plain display name
        QRegularExpression markDownURLS(
            R"(\[(?<name>.+)\]\(.+\))", QRegularExpression::MultilineOption);
        auto markDownURL = markDownURLS.globalMatch(result);
        replacements.clear();
        while (markDownURL.hasNext()) {
            auto match = markDownURL.next();
            replacements.emplace_back(
                match.capturedStart(), match.capturedLength(), match.captured("name"));
        }
        for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
            auto start = std::get<0>(*it);
            auto len = std::get<1>(*it);
            result.replace(start, len, std::get<2>(*it));
        }
    }

    { // replaces ": http(s)://..." links with "."
        QRegularExpression colonHttp(R"(:\s(https?://\S+))", QRegularExpression::MultilineOption);
        result.replace(colonHttp, ".");
    }

    { // removes "See http(s)://..." links
        QRegularExpression seeHttp(
            R"((^|(See)?\s)(https?://\S+))", QRegularExpression::MultilineOption);
        result.replace(seeHttp, "");
    }

    { // removes free standing "\c" and "\a"
        QRegularExpression moreStuff(
            R"((?>^|\s)((\\c)|(\\a))\s)", QRegularExpression::MultilineOption);
        result.replace(moreStuff, " ");
    }

    {
        QRegularExpression evenMoreStuff(R"(\bsee .)", QRegularExpression::MultilineOption);
        result.replace(evenMoreStuff, "");
    }

    {
        QRegularExpression note(R"(\\note\s)", QRegularExpression::MultilineOption);
        result.replace(note, "");
    }

    result = result.trimmed();
    cache[doc] = result;
    return result;
}

} // namespace

namespace MAXUSD_NS_DEF {
namespace ufe {

class QmaxUsdUfeDoubleSpinner : public MaxSDK::QmaxMultiSpinner
{
public:
    QmaxUsdUfeDoubleSpinner(
        int      numSpinners,
        int      numCols,
        bool     isIntegralType,
        QWidget* parent = nullptr)
        : QmaxMultiSpinner(numSpinners, numCols, parent)
    {
        if (isIntegralType) {
            for (auto s : findChildren<MaxSDK::QmaxDoubleSpinBox*>()) {
                s->setDecimals(0);
            }
        }
    }

    virtual QVector<double> fromMaxTypeVariant(const QVariant& value) const
    {
        return value.value<QVector<double>>();
    };

    virtual QVariant toMaxTypeVariant(const QVector<double>& spinnerValues) const
    {
        return QVariant::fromValue(spinnerValues);
    }

    virtual void setIndeterminate(bool indeterminate)
    {
        for (auto s : findChildren<MaxSDK::QmaxDoubleSpinBox*>()) {
            s->setIndeterminate(indeterminate);
        }
    }
};

class QmaxUsdUfeAttributesWidgetObserver : public Ufe::Observer
{
public:
    QmaxUsdUfeAttributesWidgetObserver(QmaxUsdUfeAttributesWidgetPrivate* p)
        : _p(p)
    {
    }

    void operator()(const Ufe::Notification& notification) override;

    QmaxUsdUfeAttributesWidgetPrivate* _p = nullptr;
};

class QmaxUsdUfeAttributesWidgetPrivate : public TimeChangeCallback
{
public:
    QmaxUsdUfeAttributesWidgetPrivate(QmaxUsdUfeAttributesWidget* q)
        : q_ptr(q)
        , _observer(std::make_shared<QmaxUsdUfeAttributesWidgetObserver>(this))
    {
        Ufe::Scene::instance().addObserver(_observer);
        GetCOREInterface()->RegisterTimeChangeCallback(this);

        // register the widget for notifications on time range or anim changes
        RegisterNotification(NotifyTimeRangeChanged, this, NOTIFY_TIMERANGE_CHANGE);
        RegisterNotification(
            NotifyStageAnimParameterChanged, this, NOTIFY_STAGE_ANIM_PARAMETERS_CHANGED);
    }

    ~QmaxUsdUfeAttributesWidgetPrivate()
    {
        _observer->_p = nullptr;
        for (const auto& item : _observedAttributesSceneItems) {
            Ufe::Attributes::removeObserver(item.second, _observer);
        }
        _observedAttributesSceneItems.clear();
        Ufe::Scene::instance().removeObserver(_observer);

        // making sure the timer is not executing any more after this object
        // being destroyed!
        *_callbacksQueued = false;
        GetCOREInterface()->UnRegisterTimeChangeCallback(this);

        // unregister widget from general notification system
        UnRegisterNotification(NotifyTimeRangeChanged, this, NOTIFY_TIMERANGE_CHANGE);
        UnRegisterNotification(
            NotifyStageAnimParameterChanged, this, NOTIFY_STAGE_ANIM_PARAMETERS_CHANGED);
    }

    void TimeChanged(TimeValue /* t */) override { RefreshItems(); }
    static void NotifyTimeRangeChanged(void* param, NotifyInfo* /*info*/)
    {
        const auto ufeAttributeWidget = static_cast<QmaxUsdUfeAttributesWidgetPrivate*>(param);
        ufeAttributeWidget->RefreshItems();
    }
    static void NotifyStageAnimParameterChanged(void* param, NotifyInfo* /*info*/)
    {
        const auto ufeAttributeWidget = static_cast<QmaxUsdUfeAttributesWidgetPrivate*>(param);
        ufeAttributeWidget->RefreshItems();
    }

    void observeAttributeValueChanged(
        const Ufe::Selection&        selection,
        const std::string&           attributeName,
        const std::function<void()>& callback);
    void observeSceneObjectAddOrRemoved(
        const Ufe::Selection&        selection,
        const std::function<void()>& callback);
    void observeSceneSubtreeInvalidate(
        const Ufe::Selection&        selection,
        const std::function<void()>& callback);

    /** The returned control has the display name of the first attribute set as
     * the qt object name. */
    QWidget* addControl(const Ufe::Selection& selection, const std::string& attributeName);

    // Below are templates for supporting spinners for N Dimensional UFE attributes such as
    // Float, Float2, Float3, Float4, Int, Int3, Double, Double3, Color3, Color4...

    template <
        // The type of the UFE attribute (e.g. : "Ufe::AttributeFloat3").
        typename UfeAttrType,
        // // Raw basic c type backing the attribute (e.g. int/float/double).
        typename CType,
        // Dimension of the attribute (e.g. 1 for AttributeFloat, 3 for AttributeFloat3).
        int Dimension,
        // The UFE type backing the attribute, auto-deduced from the attribute type.
        typename UfeType = std::decay_t<decltype(std::declval<UfeAttrType>().get())>,
        // Whether the c type is an integral type (vs floating).
        bool isIntegral = std::is_integral<CType>::value>
    QWidget* buildMultiSpinnerWidget(
        std::vector<Ufe::Attribute::Ptr> attributes,
        const std::string&               attributeName,
        const Ufe::Path&                 itemPath,
        const Ufe::Selection&            selection,
        QmaxUsdUfeAttributesWidget*      q)
    {
        auto numericAttributes = castAttributes<UfeAttrType>(attributes);
        if (numericAttributes.empty()) {
            return nullptr;
        }

        QPointer<QmaxUsdUfeDoubleSpinner> spinBox
            = new QmaxUsdUfeDoubleSpinner(Dimension, 1, isIntegral, q);
        spinBox->setMinimum(std::numeric_limits<CType>::lowest());
        spinBox->setMaximum(std::numeric_limits<CType>::max());
        spinBox->setMinimumWidth(100);
        auto name = QString::fromStdString(numericAttributes.front()->displayName()).simplified();
        spinBox->setObjectName(name);
        auto tooltip = cleanDocumentation(numericAttributes.front()->documentation());
        tooltip = tooltip.isEmpty() ? name : (name + "\n\n" + tooltip);
        spinBox->setToolTip(tooltip);
        spinBox->setToolTipDuration(-1);

        std::shared_ptr<bool> updating = std::make_shared<bool>(false);

        auto updateUI = [spinBox, numericAttributes, updating]() {
            if (!spinBox) {
                return;
            }
            auto value = commonValue(numericAttributes);
            *updating = true;
            if (value) {
                QVector<double> val = getSpinnerValues(*value);
                spinBox->setIndeterminate(false);
                spinBox->setValue(spinBox->toMaxTypeVariant(val));
            } else {
                spinBox->setIndeterminate(true);
            }
            *updating = false;
        };
        updateUI();

        std::shared_ptr<bool>    isInteractive = std::make_shared<bool>(false);
        std::shared_ptr<UfeType> ufeValueInit = std::make_shared<UfeType>();

        QObject::connect(
            spinBox,
            &MaxSDK::QmaxMultiSpinner::interactiveChanged,
            [isInteractive, itemPath, numericAttributes, attributeName, ufeValueInit, spinBox](
                bool interactive) {
                // Start of an interactive edit, store the initial value of the attribute.
                if (!*isInteractive && interactive) {

                    auto spinnerValues = spinBox->fromMaxTypeVariant(spinBox->value());
                    setUfeValue(spinnerValues, *ufeValueInit);
                }

                // End of an interactive edit...
                if (*isInteractive && !interactive) {

                    UfeType ufeValueCurr;
                    auto    spinnerValues = spinBox->fromMaxTypeVariant(spinBox->value());
                    setUfeValue(spinnerValues, ufeValueCurr);

                    // Revert to initial to undo from the correct value...
                    for (const auto& a : numericAttributes) {
                        a->set(*ufeValueInit);
                    }
                    // Apply the new value from an undoable command.
                    applyChanges(itemPath, numericAttributes, attributeName, ufeValueCurr);
                }
                *isInteractive = interactive;
            });

        QObject::connect(
            spinBox,
            &MaxSDK::QmaxMultiSpinner::valueChanged,
            [itemPath,
             numericAttributes,
             updating,
             updateUI,
             attributeName,
             spinBox,
             isInteractive](QVariant value) {
                if (*updating || !spinBox) {
                    return;
                }
                auto spinnerValues = spinBox->fromMaxTypeVariant(value);

                UfeType ufeValue;
                setUfeValue(spinnerValues, ufeValue);

                // In interactive mode, simply set the values and refresh the viewport.
                if (*isInteractive) {
                    for (const auto& a : numericAttributes) {
                        a->set(ufeValue);
                    }
                    GetCOREInterface()->RedrawViews(GetCOREInterface()->GetTime());
                }
                // In non-interactive mode, apply the changes through undoable commands.
                else {
                    applyChanges(itemPath, numericAttributes, attributeName, ufeValue);
                }
                // The value set may not be the stronger opinion or the attribute could be
                // uneditable...
                updateUI();
            });

        observeAttributeValueChanged(selection, attributeName, updateUI);

        if (Dimension > 1) {
            if (GetCOREInterface() && GetCOREInterface()->GetQmaxMainWindow()
                && GetCOREInterface()->GetQmaxMainWindow()->style()) {
                int s = GetCOREInterface()->GetQmaxMainWindow()->style()->pixelMetric(
                    QStyle::PM_LayoutVerticalSpacing);
                spinBox->setContentsMargins(0, s, 0, s);
            }
        }
        return spinBox;
    }

    // Helper templates to retrieve the N dimensional arrays backing some UFE attribute types.
    // Ufe Vector and Color types are both backed by std::arrays.
    template <typename CType, int Dimension>
    static std::array<CType, Dimension>& getArrayValue(Ufe::TypedColorN<CType, Dimension>& ufeColor)
    {
        return ufeColor.color;
    }

    template <typename CType, int Dimension>
    static std::array<CType, Dimension>&
    getArrayValue(Ufe::TypedVectorN<CType, Dimension>& ufeVector)
    {
        return ufeVector.vector;
    }

    // Helper templates to get values to assign to the spinners. We need a template
    // for raw types (e.g AttributeFloat, Ufe::AttributeInt, etc.) and
    // another template for array backed type (from Ufe::AttributeFloat3)
    template <
        class CType,
        typename std::enable_if<
            (std::is_floating_point<CType>::value
             || std::is_integral<CType>::value)>::type* = nullptr>
    static QVector<double> getSpinnerValues(CType& ufeVal)
    {
        QVector<double> val { static_cast<double>(ufeVal) };
        return val;
    }

    template <
        typename ArrayBackedType,
        typename std::enable_if<
            (!std::is_floating_point<ArrayBackedType>::value
             && !std::is_integral<ArrayBackedType>::value)>::type* = nullptr>
    static QVector<double> getSpinnerValues(ArrayBackedType& ufeVal)
    {
        QVector<double> val;
        const auto&     arr = getArrayValue(ufeVal);
        for (int i = 0; i < arr.size(); ++i) {
            val.push_back(arr[i]);
        }
        return val;
    }

    // Helper templates to set UFE attributes from current spinner values. We need
    // a template for raw types (e.g AttributeFloat, Ufe::AttributeInt, etc.) and
    // another template for array backed type (from Ufe::AttributeFloat3)
    template <
        class CType,
        typename std::enable_if<
            (std::is_floating_point<CType>::value
             || std::is_integral<CType>::value)>::type* = nullptr>
    static void setUfeValue(const QVector<double>& spinnerVals, CType& ufeVal)
    {
        ufeVal = static_cast<CType>(spinnerVals[0]);
    }

    template <
        typename ArrayBackedType,
        typename std::enable_if<
            (!std::is_floating_point<ArrayBackedType>::value
             && !std::is_integral<ArrayBackedType>::value)>::type* = nullptr>
    static void setUfeValue(const QVector<double>& spinnerVals, ArrayBackedType& ufeVal)
    {
        auto& arr = getArrayValue(ufeVal);
        for (int i = 0; i < spinnerVals.size(); ++i) {
            arr[i] = spinnerVals[i];
        }
    }

private:
    QmaxUsdUfeAttributesWidget* q_ptr = nullptr;
    Q_DECLARE_PUBLIC(QmaxUsdUfeAttributesWidget);

    std::unordered_map<Ufe::Path, Ufe::SceneItem::Ptr>  _observedAttributesSceneItems;
    std::shared_ptr<QmaxUsdUfeAttributesWidgetObserver> _observer;

    std::unordered_map<std::string, std::function<void()>> _attributeValueChangedCallbacks;
    std::vector<std::function<void()>>                     _objectAddOrDeleteCallbacks;
    std::vector<std::function<void()>>                     _subtreeInvalidateCallbacks;

    std::unordered_multimap<Ufe::Path, int> _objectAddOrDeleteCallbackIndices;
    std::unordered_multimap<Ufe::Path, int> _subtreeInvalidateCallbackIndices;

    std::unordered_set<int>         _queuedObjectAddOrDeleteCallbackIndices;
    std::unordered_set<int>         _queuedSubtreeInvalidateCallbackIndices;
    std::unordered_set<std::string> _queuedAttributeChangedCallbacks;

    std::shared_ptr<bool> _callbacksQueued = std::make_shared<bool>(false);
    void                  queueCallbacks();

    void RefreshItems();

    friend class QmaxUsdUfeAttributesWidgetObserver;
};

void QmaxUsdUfeAttributesWidgetObserver::operator()(const Ufe::Notification& notification)
{
    if (!_p) {
        return;
    }

    bool changed = false;

    if (auto attrNotification = dynamic_cast<const Ufe::AttributeChanged*>(&notification)) {
        changed = _p->_queuedAttributeChangedCallbacks.insert(attrNotification->name()).second;
    }

    if (!_p->_objectAddOrDeleteCallbacks.empty()) {
        if (auto objectAdd = dynamic_cast<const Ufe::ObjectAdd*>(&notification)) {
            auto range
                = _p->_objectAddOrDeleteCallbackIndices.equal_range(objectAdd->changedPath());
            for (auto it = range.first; it != range.second; ++it) {
                changed |= _p->_queuedObjectAddOrDeleteCallbackIndices.insert(it->second).second;
            }
        } else if (
            auto objectPostDelete = dynamic_cast<const Ufe::ObjectPostDelete*>(&notification)) {
            auto range = _p->_objectAddOrDeleteCallbackIndices.equal_range(
                objectPostDelete->changedPath());
            for (auto it = range.first; it != range.second; ++it) {
                changed |= _p->_queuedObjectAddOrDeleteCallbackIndices.insert(it->second).second;
            }
        }
    }

    if (!_p->_subtreeInvalidateCallbacks.empty()) {
        if (auto subtreeInvalidate = dynamic_cast<const Ufe::SubtreeInvalidate*>(&notification)) {
            auto range = _p->_subtreeInvalidateCallbackIndices.equal_range(
                subtreeInvalidate->changedPath());
            for (auto it = range.first; it != range.second; ++it) {
                changed |= _p->_queuedSubtreeInvalidateCallbackIndices.insert(it->second).second;
            }
        }
    }

    if (changed) {
        _p->queueCallbacks();
    }
}

void QmaxUsdUfeAttributesWidgetPrivate::observeAttributeValueChanged(
    const Ufe::Selection&        selection,
    const std::string&           attributeName,
    const std::function<void()>& callback)
{
    _attributeValueChangedCallbacks[attributeName] = callback;

    for (const auto& item : selection) {
        if (_observedAttributesSceneItems.emplace(item->path(), item).second) {
            Ufe::Attributes::addObserver(item, _observer);
        }
    }
}

void QmaxUsdUfeAttributesWidgetPrivate::observeSceneObjectAddOrRemoved(
    const Ufe::Selection&        selection,
    const std::function<void()>& callback)
{
    int idx = static_cast<int>(_objectAddOrDeleteCallbacks.size());
    _objectAddOrDeleteCallbacks.emplace_back(callback);

    for (const auto& item : selection) {
        _objectAddOrDeleteCallbackIndices.emplace(item->path(), idx);
    }
}

void QmaxUsdUfeAttributesWidgetPrivate::observeSceneSubtreeInvalidate(
    const Ufe::Selection&        selection,
    const std::function<void()>& callback)
{
    int idx = static_cast<int>(_subtreeInvalidateCallbacks.size());
    _subtreeInvalidateCallbacks.emplace_back(callback);

    for (const auto& item : selection) {
        _subtreeInvalidateCallbackIndices.emplace(item->path(), idx);
    }
}

void QmaxUsdUfeAttributesWidgetPrivate::queueCallbacks()
{
    if (*_callbacksQueued || !_callbacksQueued) {
        return;
    }
    *_callbacksQueued = true;

    std::shared_ptr<bool> queued = _callbacksQueued;
    QTimer::singleShot(0, [this, queued] {
        if (*queued) {
            std::unordered_set<int>         objectAddOrDeleteQueue;
            std::unordered_set<int>         subtreeInvalidateQueue;
            std::unordered_set<std::string> attributeValueChangedQueue;

            std::swap(objectAddOrDeleteQueue, _queuedObjectAddOrDeleteCallbackIndices);
            std::swap(subtreeInvalidateQueue, _queuedSubtreeInvalidateCallbackIndices);
            std::swap(attributeValueChangedQueue, _queuedAttributeChangedCallbacks);

            *queued = false;

            for (auto idx : objectAddOrDeleteQueue) {
                _objectAddOrDeleteCallbacks[idx]();
            }

            for (auto idx : subtreeInvalidateQueue) {
                _subtreeInvalidateCallbacks[idx]();
            }

            for (const auto& a : attributeValueChangedQueue) {
                auto it = _attributeValueChangedCallbacks.find(a);
                if (it != _attributeValueChangedCallbacks.end()) {
                    it->second();
                }
            }
        }
    });
}

void QmaxUsdUfeAttributesWidgetPrivate::RefreshItems() {
    for (auto items : _attributeValueChangedCallbacks) {
        items.second();
    }
}


QWidget* QmaxUsdUfeAttributesWidgetPrivate::addControl(
    const Ufe::Selection& selection,
    const std::string&    attributeName)
{
    Q_Q(QmaxUsdUfeAttributesWidget);

    // check if the attributes with the given name of the selection are the same type
    std::vector<Ufe::Attribute::Ptr>         attributes;
    std::unordered_set<Ufe::Attribute::Type> attributeTypes;
    Ufe::Path                                itemPath;
    for (const auto& item : selection) {
        auto itemAttributes = Ufe::Attributes::attributes(item);
        auto attr = itemAttributes->attribute(attributeName);
        if (attr) {
            // We use the first (non-empty) path of an item for the creation of
            // the undo command, as all of our selected sub-objects are assumed
            // to share the same edit target.
            if (itemPath.empty()) {
                itemPath = item->path();
            }
            attributes.emplace_back(attr);
            attributeTypes.insert(attr->type());
        } else {
            return nullptr;
        }
    }
    // There are no attributes with the given name at all or they have different
    // types. In both cases there's nothing we can do -> outa here!
    if (attributeTypes.size() != 1) {
        return nullptr;
    }

    Ufe::Attribute::Type type = *attributeTypes.begin();

    // Now comes the fun part :)
    // -------------------------------------------------------------------------
    // BOOL
    // -------------------------------------------------------------------------
    if (type == Ufe::Attribute::kBool) {
        auto boolAttributes = castAttributes<Ufe::AttributeBool>(attributes);
        if (boolAttributes.empty()) {
            return nullptr;
        }

        QPointer<QCheckBox> checkBox = new QCheckBox(q);
        auto name = QString::fromStdString(boolAttributes.front()->displayName()).simplified();
        checkBox->setObjectName(name);
        auto tooltip = cleanDocumentation(boolAttributes.front()->documentation());
        tooltip = tooltip.isEmpty() ? name : (name + "\n\n" + tooltip);
        checkBox->setToolTip(tooltip);
        checkBox->setToolTipDuration(-1);

        std::shared_ptr<bool> updating = std::make_shared<bool>(false);

        auto updateUI = [checkBox, boolAttributes, updating]() {
            if (!checkBox) {
                return;
            }
            auto value = commonValue(boolAttributes);
            *updating = true;
            if (value) {
                checkBox->setCheckState(
                    *value ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
            } else {
                checkBox->setCheckState(Qt::CheckState::PartiallyChecked);
            }
            *updating = false;
        };
        updateUI();

        QObject::connect(
            checkBox,
            &QCheckBox::clicked,
            [itemPath, boolAttributes, updating, updateUI, attributeName](bool checked) {
                if (*updating) {
                    return;
                }
                applyChanges(itemPath, boolAttributes, attributeName, checked);
                // The value set may not be the stronger opinion or the attribute could be
                // uneditable...
                updateUI();
            });

        observeAttributeValueChanged(selection, attributeName, updateUI);
        return checkBox;
    }

    // -------------------------------------------------------------------------
    // INT N (only Int and Int3 exist for attributes)
    // -------------------------------------------------------------------------
    if (type == Ufe::Attribute::kInt) {
        return buildMultiSpinnerWidget<Ufe::AttributeInt, int, 1>(
            attributes, attributeName, itemPath, selection, q);
    }
    if (type == Ufe::Attribute::kInt3) {
        return buildMultiSpinnerWidget<Ufe::AttributeInt3, int, 3>(
            attributes, attributeName, itemPath, selection, q);
    }

    // -------------------------------------------------------------------------
    // FLOAT N
    // -------------------------------------------------------------------------
    if (type == Ufe::Attribute::kFloat) {
        return buildMultiSpinnerWidget<Ufe::AttributeFloat, float, 1>(
            attributes, attributeName, itemPath, selection, q);
    }
    if (type == Ufe::Attribute::kFloat2) {
        return buildMultiSpinnerWidget<Ufe::AttributeFloat2, float, 2>(
            attributes, attributeName, itemPath, selection, q);
    }
    if (type == Ufe::Attribute::kFloat3) {
        return buildMultiSpinnerWidget<Ufe::AttributeFloat3, float, 3>(
            attributes, attributeName, itemPath, selection, q);
    }
    if (type == Ufe::Attribute::kFloat4) {
        return buildMultiSpinnerWidget<Ufe::AttributeFloat4, float, 4>(
            attributes, attributeName, itemPath, selection, q);
    }

    // -------------------------------------------------------------------------
    // DOUBLE N (only double and double3 exist for attribute types)
    // -------------------------------------------------------------------------
    if (type == Ufe::Attribute::kDouble) {
        return buildMultiSpinnerWidget<Ufe::AttributeDouble, double, 1>(
            attributes, attributeName, itemPath, selection, q);
    }
    if (type == Ufe::Attribute::kDouble3) {
        return buildMultiSpinnerWidget<Ufe::AttributeDouble3, double, 3>(
            attributes, attributeName, itemPath, selection, q);
    }

    // -------------------------------------------------------------------------
    // COLOR N
    // -------------------------------------------------------------------------
    if (type == Ufe::Attribute::kColorFloat3) {
        return buildMultiSpinnerWidget<Ufe::AttributeColorFloat3, float, 3>(
            attributes, attributeName, itemPath, selection, q);
    }
    if (type == Ufe::Attribute::kColorFloat4) {
        return buildMultiSpinnerWidget<Ufe::AttributeColorFloat4, float, 4>(
            attributes, attributeName, itemPath, selection, q);
    }

    // -------------------------------------------------------------------------
    // String
    // -------------------------------------------------------------------------
    if (type == Ufe::Attribute::kString) {
        auto stringAttributes = castAttributes<Ufe::AttributeString>(attributes);
        if (stringAttributes.empty()) {
            return nullptr;
        }

        QPointer<QLineEdit> lineEdit = new QLineEdit(q);

        auto name = QString::fromStdString(stringAttributes.front()->displayName()).simplified();
        lineEdit->setObjectName(name);
        auto tooltip = cleanDocumentation(stringAttributes.front()->documentation());
        tooltip = tooltip.isEmpty() ? name : (name + "\n\n" + tooltip);
        lineEdit->setToolTip(tooltip);
        lineEdit->setToolTipDuration(-1);

        std::shared_ptr<bool> updating = std::make_shared<bool>(false);

        auto updateUI = [lineEdit, stringAttributes, updating]() {
            if (!lineEdit) {
                return;
            }
            auto value = commonValue(stringAttributes);
            *updating = true;
            if (value) {
                lineEdit->setText(QString::fromStdString(*value));
            } else {
                lineEdit->setText("");
            }
            *updating = false;
        };
        updateUI();

        QObject::connect(
            lineEdit,
            &QLineEdit::editingFinished,
            [itemPath, lineEdit, stringAttributes, updating, updateUI, attributeName]() {
                if (*updating || !lineEdit) {
                    return;
                }
                auto value = lineEdit->text().toStdString();
                auto prevValue = commonValue(stringAttributes);
                if (prevValue && *prevValue == value) {
                    return;
                }
                applyChanges(itemPath, stringAttributes, attributeName, value);
                // The value set may not be the stronger opinion or the attribute could be
                // uneditable...
                updateUI();
            });

        observeAttributeValueChanged(selection, attributeName, updateUI);

        return lineEdit;
    }

    // -------------------------------------------------------------------------
    // Filename
    // -------------------------------------------------------------------------
    if (type == Ufe::Attribute::kFilename) {
        auto filenameAttributes = castAttributes<Ufe::AttributeFilename>(attributes);
        if (filenameAttributes.empty()) {
            return nullptr;
        }

        QPointer<UfeUi::QFilenameEdit> filenameEdit = new UfeUi::QFilenameEdit(q);

        auto name = QString::fromStdString(filenameAttributes.front()->displayName()).simplified();
        filenameEdit->setObjectName(name);
        auto tooltip = cleanDocumentation(filenameAttributes.front()->documentation());
        tooltip = tooltip.isEmpty() ? name : (name + "\n\n" + tooltip);
        filenameEdit->setToolTip(tooltip);
        filenameEdit->setToolTipDuration(-1);

        filenameEdit->setCaption(QApplication::translate("USDStageObject", "Choose %1").arg(name));
        if (!selection.empty()) {
            if (auto prim = selection.front()) {
                const auto& primPath = prim->path();
                if (primPath.nbSegments() > 1) {
                    auto stagePath = primPath.head(1);

                    if (auto stage = MaxUsd::ufe::getStage(stagePath)) {
                        auto targetLayer = stage->GetEditTarget().GetLayer();
                        if (!targetLayer->IsAnonymous()) {
                            auto s = QString::fromStdString(targetLayer->GetRealPath());
                            s = QFileInfo(s).absolutePath();
                            filenameEdit->setInitialDirectory(s);
                            filenameEdit->setBaseDirectory(s);
                        }
                    }
                }
            }
        }

        std::shared_ptr<bool> updating = std::make_shared<bool>(false);

        auto updateUI = [filenameEdit, filenameAttributes, updating]() {
            if (!filenameEdit) {
                return;
            }
            auto value = commonValue(filenameAttributes);
            *updating = true;
            if (value) {
                filenameEdit->setFilename(QString::fromStdString(*value));
            } else {
                filenameEdit->setFilename("");
            }
            *updating = false;
        };
        updateUI();

        QObject::connect(
            filenameEdit,
            &UfeUi::QFilenameEdit::filenameChanged,
            [itemPath, filenameEdit, filenameAttributes, updating, updateUI, attributeName](
                const QString& filename) {
                if (*updating || !filenameEdit) {
                    return;
                }
                auto value = filename.toStdString();
                auto prevValue = commonValue(filenameAttributes);
                if (prevValue && *prevValue == value) {
                    return;
                }
                applyChanges(itemPath, filenameAttributes, attributeName, value);
                // The value set may not be the stronger opinion or the attribute could be
                // uneditable...
                updateUI();
            });

        observeAttributeValueChanged(selection, attributeName, updateUI);

        return filenameEdit;
    }

    // -------------------------------------------------------------------------
    // Enum String
    // -------------------------------------------------------------------------
    if (type == Ufe::Attribute::kEnumString) {
        auto enumAttributes = castAttributes<Ufe::AttributeEnumString>(attributes);
        if (enumAttributes.empty()) {
            return nullptr;
        }

        QPointer<QComboBox> comboBox = new QComboBox(q);
        comboBox->setMinimumWidth(100);

        auto name = QString::fromStdString(enumAttributes.front()->displayName()).simplified();
        comboBox->setObjectName(name);
        auto tooltip = cleanDocumentation(enumAttributes.front()->documentation());
        tooltip = tooltip.isEmpty() ? name : (name + "\n\n" + tooltip);
        comboBox->setToolTip(tooltip);
        comboBox->setToolTipDuration(-1);

        std::vector<std::string> commonOptions;
        bool                     firstOne = true;
        for (const auto& attr : enumAttributes) {
            auto options = attr->getEnumValues();
            if (firstOne) {
                commonOptions = options;
                firstOne = false;
            } else {
                // remove from common if not in ancestors
                commonOptions.erase(
                    std::remove_if(
                        commonOptions.begin(),
                        commonOptions.end(),
                        [&options](const auto& it) {
                            return std::find(options.begin(), options.end(), it) == options.end();
                        }),
                    commonOptions.end());
            }
        }

        for (const auto s : commonOptions) {
            comboBox->addItem(QString::fromStdString(s));
        }

        std::shared_ptr<bool> updating = std::make_shared<bool>(false);

        auto updateUI = [comboBox, enumAttributes, updating]() {
            auto value = commonValue(enumAttributes);
            *updating = true;
            if (value) {
                comboBox->setCurrentText(QString::fromStdString(*value));
            } else {
                comboBox->setCurrentIndex(-1);
            }
            *updating = false;
        };
        updateUI();

        QObject::connect(
            comboBox,
            &QComboBox::currentTextChanged,
            [itemPath, enumAttributes, updating, updateUI, attributeName](QString value) {
                if (*updating) {
                    return;
                }
                applyChanges(itemPath, enumAttributes, attributeName, value.toStdString());
                // The value set may not be the stronger opinion or the attribute could be
                // uneditable...
                updateUI();
            });

        observeAttributeValueChanged(selection, attributeName, updateUI);

        return comboBox;
    }

    if (type == Ufe::Attribute::kGeneric) {
        auto genericAttributes = castAttributes<Ufe::AttributeGeneric>(attributes);
        if (genericAttributes.empty()) {
            return nullptr;
        }

        qDebug() << "Unknown type" << QString::fromStdString(type) << "for attribute"
                 << QString::fromStdString(attributeName) << "native type"
                 << QString::fromStdString(
                        genericAttributes.front()
                            ->nativeType()); // << "value" << genericAttributes.front()->string();
    } else {
        qDebug() << "Unknown type" << QString::fromStdString(type) << "for attribute"
                 << QString::fromStdString(attributeName);
    }

    return nullptr;
}

QmaxUsdUfeAttributesWidget::QmaxUsdUfeAttributesWidget()
    : QWidget(nullptr)
    , d_ptr(new QmaxUsdUfeAttributesWidgetPrivate(this))
{
}

QmaxUsdUfeAttributesWidget::~QmaxUsdUfeAttributesWidget() { }

std::unique_ptr<QmaxUsdUfeAttributesWidget> QmaxUsdUfeAttributesWidget::create(
    const Ufe::Selection&           selection,
    const std::vector<std::string>& attributeNames,
    std::set<std::string>&          handledAttributeNames)
{
    auto widget = std::make_unique<QmaxUsdUfeAttributesWidget>();
    int  idx = 0;
    // Create controls for the attributes

    auto l = new QGridLayout(widget.get());
    l->setColumnStretch(0, 1);
    l->setColumnStretch(1, 2);

    for (const auto& name : attributeNames) {
        // Create a control for the attribute
        if (auto control = widget->d_ptr->addControl(selection, name)) {
            handledAttributeNames.insert(name);

            auto label = new ElidedLabel(control->objectName());
            label->setToolTip(control->toolTip());
            label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            // todo: fix ElidedLabel to BE an actual QLabel
            // label->setBuddy(control);
            // label->setWordWrap(true);
            l->addWidget(label, idx, 0);
            l->addWidget(control, idx++, 1);
        }
    }

    if (idx == 0) {
        widget.reset();
    }
    return widget;
}

std::unique_ptr<QmaxUsdUfeAttributesWidget> QmaxUsdUfeAttributesWidget::create(
    const Ufe::Selection&  selection,
    const TfType&          type,
    std::set<std::string>& handledAttributeNames)
{
    if (selection.empty() || !type.IsA<UsdSchemaBase>()) {
        return nullptr;
    }

    auto attributeNames = extractSchemaAttributeNames(type);
    if (attributeNames.empty()) {
        return nullptr;
    }

    auto widget = create(selection, attributeNames, handledAttributeNames);

    if (widget) {
        widget->setObjectName(rollupTitleFromTypeName(
            UsdSchemaRegistry::IsConcrete(type)
                ? UsdSchemaRegistry::GetSchemaTypeName(type).GetString()
                : type.GetTypeName()));
    }
    return std::move(widget);
}

std::unique_ptr<QmaxUsdUfeAttributesWidget> QmaxUsdUfeAttributesWidget::createMetaData(
    const Ufe::Selection&        selection,
    const std::set<std::string>& handledAttributeNames)
{
    if (selection.empty()) {
        return nullptr;
    }

    auto widget = std::make_unique<QmaxUsdUfeAttributesWidget>();
    int  idx = 0;
    // Create controls for the attributes

    auto l = new QGridLayout(widget.get());
    l->setColumnStretch(0, 1);
    l->setColumnStretch(1, 2);

    bool                     isFirst = true;
    std::vector<std::string> attributeNames;
    for (const auto& item : selection) {
        auto itemAttributes = Ufe::Attributes::attributes(item);
        auto names = itemAttributes->attributeNames();
        names.erase(
            std::remove_if(
                names.begin(),
                names.end(),
                [&handledAttributeNames](const auto name) {
                    return handledAttributeNames.find(name) != handledAttributeNames.end();
                }),
            names.end());
        if (isFirst) {
            attributeNames.insert(attributeNames.end(), names.begin(), names.end());
            isFirst = false;
        } else {
            attributeNames.erase(
                std::remove_if(
                    attributeNames.begin(),
                    attributeNames.end(),
                    [&names](const auto name) {
                        return std::find(names.begin(), names.end(), name) == names.end();
                    }),
                attributeNames.end());
            if (attributeNames.empty()) {
                break;
            }
        }
    }

    Ufe::Path                                                   itemPath;
    std::map<TfToken, std::vector<std::pair<UsdPrim, VtValue>>> combinedMetaData;
    for (const auto& item : selection) {
        auto prim = MaxUsd::ufe::ufePathToPrim(item->path());
        if (!prim) {
            continue;
        }
        // We use the first (non-empty) path of an item for the creation of
        // the undo command, as all of our selected sub-objects are assumed
        // to share the same edit target.
        if (itemPath.empty()) {
            itemPath = item->path();
        }
        auto metadata = prim.GetAllMetadata();
        for (auto& m : metadata) {
            combinedMetaData[m.first].emplace_back(std::make_pair(prim, m.second));
        }
    }

    if (selection.size() > 1) {
        for (auto it = combinedMetaData.begin(); it != combinedMetaData.end();) {
            if (it->second.size() == selection.size()) // token present in all selections
            {
                ++it;
            } else {
                it = combinedMetaData.erase(it);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Prim Path(s)
    // -------------------------------------------------------------------------
    {
        auto label = new ElidedLabel(QApplication::translate(
            "USDStageObject", selection.size() == 1 ? "Prim Path" : "Prim Paths"));

        auto textEdit = new QLineEdit();
        textEdit->setReadOnly(true);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        l->addWidget(label, idx, 0);
        l->addWidget(textEdit, idx++, 1);

        QStringList paths;
        for (const auto& item : selection) {
            if (item) {
                auto p = item->path().popHead();
                paths.append(QString::fromStdString(p.string()));
            }
        }
        textEdit->setText(paths.join(", "));
        textEdit->setToolTip(paths.join("\n"));
    }

    std::vector<UsdPrim> prims;
    for (const auto& item : selection) {
        auto prim = MaxUsd::ufe::ufePathToPrim(item->path());
        if (prim) {
            prims.emplace_back(prim);
        }
    }

    // -------------------------------------------------------------------------
    // Kind combo box
    // -------------------------------------------------------------------------
    {
        QPointer<QComboBox> comboBox = new QComboBox();
        auto label = new ElidedLabel(QApplication::translate("USDStageObject", "Kind"));
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // We add the known Kind types, in a certain order("model hierarchy")
        // and then any extra ones that were added by extending the kind
        // registry.
        // Note : we remove the "model" kind because in the USD docs it states,
        // "No prim should have the exact kind " model ".

        std::vector<TfToken> knownKinds = {
            KindTokens->group, KindTokens->assembly, KindTokens->component, KindTokens->subcomponent
        };

        for (auto kind : KindRegistry::GetInstance().GetAllKinds()) {
            if (kind == KindTokens->model) {
                continue;
            }
            if (std::find(knownKinds.begin(), knownKinds.end(), kind) == knownKinds.end()) {
                knownKinds.push_back(kind);
            }
        }

        std::shared_ptr<bool> updating = std::make_shared<bool>(false);

        auto updateUI = [comboBox, prims, updating, knownKinds]() {
            if (!comboBox) {
                return;
            }

            std::set<TfToken> primKinds;
            for (const auto& prim : prims) {
                auto    model = UsdModelAPI(prim);
                TfToken kind;
#if PXR_VERSION >= 2311
                if (prim.GetKind(&kind)) {
                    primKinds.insert(kind);
                }
#else
                if (!prim.IsPseudoRoot() && prim.GetPath() != SdfPath::AbsoluteRootPath()
                    && prim.GetMetadata(SdfFieldKeys->Kind, &kind)) {
                    primKinds.insert(kind);
                }
#endif
                else {
                    primKinds.insert(TfToken(""));
                }
            }

            std::vector<TfToken> knownKindsCopy = knownKinds;
            for (const auto& kind : primKinds) {
                if (std::find(knownKindsCopy.begin(), knownKindsCopy.end(), kind)
                    == knownKindsCopy.end()) {
                    knownKindsCopy.push_back(kind);
                }
            }

            if (std::find(knownKindsCopy.begin(), knownKindsCopy.end(), TfToken(""))
                == knownKindsCopy.end()) {
                knownKindsCopy.push_back(TfToken(""));
            }

            *updating = true;
            comboBox->clear();
            for (const auto& kind : knownKindsCopy) {
                comboBox->addItem(QString::fromStdString(kind.GetString()));
            }

            if (primKinds.size() == 1) {
                comboBox->setCurrentText(QString::fromStdString(primKinds.begin()->GetString()));
            } else {
                comboBox->setCurrentIndex(-1);
            }
            *updating = false;
        };

        QObject::connect(
            comboBox,
            &QComboBox::currentTextChanged,
            [comboBox, updating, prims, itemPath, updateUI](QString value) {
                if (*updating || !comboBox) {
                    return;
                }

                std::vector<TfToken> kindsBefore;
                for (const auto& prim : prims) {
                    auto    model = UsdModelAPI(prim);
                    TfToken kind;
#if PXR_VERSION >= 2311
                    if (prim.GetKind(&kind)) {
                        kindsBefore.emplace_back(kind);
                    }
#else
                    if (!prim.IsPseudoRoot() && prim.GetPath() != SdfPath::AbsoluteRootPath()
                        && prim.GetMetadata(SdfFieldKeys->Kind, &kind)) {
                        kindsBefore.emplace_back(kind);
                    }
#endif
                    else {
                        kindsBefore.emplace_back("");
                    }
                }

                const std::string commandName
                    = QApplication::translate("USDStageObject", "Change Kind of USD prim")
                          .toStdString();

                TfToken newKind(value.toStdString());
                applyChanges(
                    itemPath,
                    [prims, newKind, kindsBefore](UfeUI::GenericCommand::Mode mode) {
                        int i = 0;
                        for (const auto& prim : prims) {
                            if (prim) {
#if PXR_VERSION >= 2311
                                prim.SetKind(
                                    mode == UfeUI::GenericCommand::Mode::kRedo ? newKind
                                                                               : kindsBefore[i]);
#else
                                prim.SetMetadata(
                                    SdfFieldKeys->Kind,
                                    mode == UfeUI::GenericCommand::Mode::kUndo ? kindsBefore[i]
                                                                               : newKind);
#endif
                            }
                            ++i;
                        }
                    },
                    commandName);

                // We need to query the kind value again, as "someone may have a
                // stronger opinion.."
                updateUI();
            });

        widget->d_ptr->observeSceneSubtreeInvalidate(selection, updateUI);
        updateUI();

        l->addWidget(label, idx, 0);
        l->addWidget(comboBox, idx++, 1);
    }

    // -------------------------------------------------------------------------
    // Active check box
    // -------------------------------------------------------------------------
    {
        QPointer<QCheckBox> checkBox = new QCheckBox();
        auto label = new ElidedLabel(QApplication::translate("USDStageObject", "Active"));
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        std::shared_ptr<bool> updating = std::make_shared<bool>(false);

        auto updateUI = [checkBox, prims, updating]() {
            if (!checkBox) {
                return;
            }

            std::unique_ptr<bool> active;
            bool                  first = true;
            for (const auto& prim : prims) {
                bool primActive = prim.IsActive();
                if (first) {
                    active = std::make_unique<bool>(primActive);
                    first = false;
                } else {
                    if (active && *active != primActive) {
                        active.reset();
                        break;
                    }
                }
            }

            *updating = true;
            if (active) {
                checkBox->setCheckState(*active ? Qt::Checked : Qt::Unchecked);
            } else {
                checkBox->setCheckState(Qt::PartiallyChecked);
            }
            *updating = false;
        };

        QObject::connect(
            checkBox, &QCheckBox::clicked, [itemPath, prims, updating, updateUI](bool checked) {
                if (*updating || prims.empty()) {
                    return;
                }

                std::vector<bool> activeBefore;
                for (const auto& prim : prims) {
                    activeBefore.emplace_back(prim ? prim.IsActive() : false);
                }

                const std::string commandName
                    = QApplication::translate(
                          "USDStageObject", checked ? "Activate USD prim" : "Deactivate USD prim")
                          .toStdString();
                applyChanges(
                    itemPath,
                    [prims, checked, activeBefore](UfeUI::GenericCommand::Mode mode) {
                        int i = 0;
                        for (const auto& prim : prims) {
                            if (prim) {
                                prim.SetActive(
                                    mode == UfeUI::GenericCommand::Mode::kRedo ? checked
                                                                               : activeBefore[i]);
                            }
                            ++i;
                        }
                    },
                    commandName);

                // We need to query the active value again, as "someone may have a
                // stronger opinion.."
                updateUI();
            });

        widget->d_ptr->observeSceneObjectAddOrRemoved(selection, updateUI);
        updateUI();

        l->addWidget(label, idx, 0);
        l->addWidget(checkBox, idx++, 1);
    }

    // -------------------------------------------------------------------------
    // Instanceable
    // -------------------------------------------------------------------------
    {
        QPointer<QCheckBox> checkBox = new QCheckBox();
        auto label = new ElidedLabel(QApplication::translate("USDStageObject", "Instanceable"));
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        std::shared_ptr<bool> updating = std::make_shared<bool>(false);

        auto updateUI = [checkBox, prims, updating]() {
            if (!checkBox) {
                return;
            }

            std::unique_ptr<bool> instanceable;
            bool                  first = true;
            for (const auto& prim : prims) {
                if (!prim) {
                    continue;
                }
                bool primInstanceable = prim.IsInstanceable();
                if (first) {
                    instanceable = std::make_unique<bool>(primInstanceable);
                    first = false;
                } else {
                    if (instanceable && *instanceable != primInstanceable) {
                        instanceable.reset();
                        break;
                    }
                }
            }

            *updating = true;
            if (instanceable) {
                checkBox->setCheckState(*instanceable ? Qt::Checked : Qt::Unchecked);
            } else {
                checkBox->setCheckState(Qt::PartiallyChecked);
            }
            *updating = false;
        };

        QObject::connect(
            checkBox, &QCheckBox::clicked, [itemPath, prims, updating, updateUI](bool checked) {
                if (*updating) {
                    return;
                }

                std::vector<bool> instanceableBefore;
                for (const auto& prim : prims) {
                    instanceableBefore.emplace_back(prim ? prim.IsInstanceable() : false);
                }

                const std::string commandName = QApplication::translate(
                                                    "USDStageObject",
                                                    checked ? "Mark USD prim as Instanceable"
                                                            : "Unmark USD prim as Instanceable")
                                                    .toStdString();
                applyChanges(
                    itemPath,
                    [prims, checked, instanceableBefore](UfeUI::GenericCommand::Mode mode) {
                        int i = 0;
                        for (const auto& prim : prims) {
                            if (prim) {
                                prim.SetInstanceable(
                                    mode == UfeUI::GenericCommand::Mode::kRedo
                                        ? checked
                                        : instanceableBefore[i]);
                            }
                            ++i;
                        }
                    },
                    commandName);

                // We need to query the active value again, as "someone may have a
                // stronger opinion.."
                updateUI();
            });

        widget->d_ptr->observeSceneSubtreeInvalidate(selection, updateUI);
        updateUI();

        l->addWidget(label, idx, 0);
        l->addWidget(checkBox, idx++, 1);
    }

    for (const auto& meta : combinedMetaData) {
        auto& name = meta.first;
        if (name == SdfFieldKeys->Active || name == SdfFieldKeys->Kind
            || name == SdfFieldKeys->Instanceable) {
            continue;
        }

        // Create a control for the attribute
        if (auto control = widget->d_ptr->addControl(selection, name)) {
            auto label = new ElidedLabel(control->objectName());
            label->setToolTip(control->toolTip());
            label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            l->addWidget(label, idx, 0);
            l->addWidget(control, idx++, 1);
        }
    }

    // TBD: what shall we do with custom attributes ???
    // for (const auto& name : attributeNames)
    //{
    //	if (name == SdfFieldKeys->Active || name == SdfFieldKeys->Kind)
    //	{
    //		continue;
    //	}

    //	// Create a control for the attribute
    //	if (auto control = widget->d_ptr->addControl(selection, name))
    //	{
    //		auto label = new QLabel(control->objectName());
    //		label->setToolTip(control->toolTip());
    //		label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    //		label->setBuddy(control);
    //		l->addWidget(label, idx, 0);
    //		l->addWidget(control, idx++, 1);
    //	}
    //}

    widget->setObjectName(QApplication::translate("USDStageObject", "Metadata"));
    return widget;
}

} // namespace ufe
} // namespace MAXUSD_NS_DEF
