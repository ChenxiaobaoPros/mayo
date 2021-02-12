/****************************************************************************
** Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#include "property_item_delegate.h"
#include "../base/application.h"
#include "../base/property_builtins.h"
#include "../base/settings.h"
#include "../base/string_utils.h"
#include "../base/unit_system.h"
#include "../gui/qtgui_utils.h"
#include "app_module.h"
#include "theme.h"

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

namespace Mayo {

namespace {

static QString toStringDHMS(QuantityTime time)
{
    const double duration_s = UnitSystem::seconds(time);
    const double days = duration_s / 86400.;
    const int dayCount = std::floor(days);
    const double hours = (days - dayCount) * 24;
    const int hourCount = std::floor(hours);
    const double mins = (hours - hourCount) * 60;
    const int minCount = std::floor(mins);
    const double secs = (mins - minCount) * 60;
    const int secCount = std::floor(secs);
    QString text;
    if (dayCount > 0)
        text += PropertyItemDelegate::tr("%1d ").arg(dayCount);

    if (hourCount > 0)
        text += PropertyItemDelegate::tr("%1h ").arg(hourCount);

    if (minCount > 0)
        text += PropertyItemDelegate::tr("%1min ").arg(minCount);

    if (secCount > 0)
        text += PropertyItemDelegate::tr("%1s").arg(secCount);

    return text.trimmed();
}

static QString propertyValueText(const PropertyBool* prop) {
    return StringUtils::yesNoText(*prop);
}

static QString propertyValueText(const PropertyInt* prop) {
    return Application::instance()->settings()->locale().toString(prop->value());
}

static QString propertyValueText(const PropertyDouble* prop) {
    return StringUtils::text(prop->value(), AppModule::get(Application::instance())->defaultTextOptions());
}

static QString propertyValueText(const PropertyCheckState* prop) {
    return StringUtils::yesNoText(*prop);
}

static QString propertyValueText(const PropertyQByteArray* prop) {
    return QString::fromUtf8(prop->value());
}

static QString propertyValueText(const PropertyQString* prop) {
    return prop->value();
}

static QString propertyValueText(const PropertyQDateTime* prop) {
    return Application::instance()->settings()->locale().toString(prop->value());
}

static QString propertyValueText(const PropertyOccColor* prop) {
    return StringUtils::text(prop->value());
}

static QString propertyValueText(const PropertyOccPnt* prop) {
    return StringUtils::text(prop->value(), AppModule::get(Application::instance())->defaultTextOptions());
}

static QString propertyValueText(const PropertyOccTrsf* prop) {
    return StringUtils::text(prop->value(), AppModule::get(Application::instance())->defaultTextOptions());
}

static QString propertyValueText(const PropertyEnumeration* prop)
{
    for (const Enumeration::Item& enumItem : prop->enumeration().items()) {
        if (enumItem.value == prop->value())
            return enumItem.name.tr();
    }

    return QString();
}

static QString propertyValueText(const BasePropertyQuantity* prop)
{
    if (prop->quantityUnit() == Unit::Time) {
        auto propTime = static_cast<const PropertyTime*>(prop);
        return toStringDHMS(propTime->quantity());
    }

    const UnitSystem::TranslateResult trRes = PropertyEditorFactory::unitTranslate(prop);
    return PropertyItemDelegate::tr("%1%2")
            .arg(StringUtils::text(trRes.value, AppModule::get(Application::instance())->defaultTextOptions()))
            .arg(trRes.strUnit);
}

static QString propertyValueText(
        const BasePropertyQuantity* prop,
        const PropertyItemDelegate::UnitTranslation& unitTr)
{
    const double trValue = prop->quantityValue() * unitTr.factor;
    return PropertyItemDelegate::tr("%1%2")
            .arg(StringUtils::text(trValue, AppModule::get(Application::instance())->defaultTextOptions()))
            .arg(unitTr.strUnit);
}

} // namespace

PropertyItemDelegate::PropertyItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent),
      m_editorFactory(new DefaultPropertyEditorFactory)
{}

void PropertyItemDelegate::setPropertyEditorFactory(std::unique_ptr<PropertyEditorFactory> editorFactory)
{
    m_editorFactory = std::move(editorFactory);
}

bool PropertyItemDelegate::overridePropertyUnitTranslation(
        const BasePropertyQuantity* prop, PropertyItemDelegate::UnitTranslation unitTr)
{
    if (!prop || prop->quantityUnit() != unitTr.unit)
        return false;

    m_mapPropUnitTr.emplace(prop, unitTr);
    return true;
}

void PropertyItemDelegate::paint(
        QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (index.column() == 1) {
        const Property* prop = qvariant_cast<Property*>(index.data());
        if (prop && prop->dynTypeName() == PropertyOccColor::TypeName) {
            auto propColor = static_cast<const PropertyOccColor*>(prop);
            painter->save();

            QApplication::style()->drawPrimitive(
                        QStyle::PE_PanelItemViewItem,
                        &option,
                        painter,
                        option.widget);

            const QColor color = QtGuiUtils::toQColor(propColor->value());
            const QPixmap pixColor = PropertyEditorFactory::colorSquarePixmap(color, option.rect.height());
            painter->drawPixmap(option.rect.x(), option.rect.y(), pixColor);
            const QString strColor = propertyValueText(propColor);

            QRect labelRect = option.rect;
            labelRect.setX(option.rect.x() + pixColor.width() + 6);
            QApplication::style()->drawItemText(
                        painter,
                        labelRect,
                        Qt::AlignLeft | Qt::AlignVCenter,
                        option.palette,
                        option.state.testFlag(QStyle::State_Enabled),
                        strColor);

            painter->restore();
            return;
        }
    }

    QStyledItemDelegate::paint(painter, option, index);
    if (index.column() == 1 && !index.data().isNull()) {
        const Property* prop = qvariant_cast<Property*>(index.data());
        if (!prop->isUserReadOnly()
                && option.state.testFlag(QStyle::State_Enabled)
                && option.state.testFlag(QStyle::State_MouseOver))
        {
            const QSize itemSize = this->sizeHint(option, index);
            const QSize pixItemSize = option.decorationSize * 0.75;
            const QPixmap pixEdit = mayoTheme()->icon(Theme::Icon::Edit).pixmap(pixItemSize);
            painter->drawPixmap(
                        option.rect.x() + itemSize.width() + 4,
                        option.rect.y() + (itemSize.height() - pixItemSize.height()) / 2.,
                        pixEdit);
        }
    }
}

QString PropertyItemDelegate::displayText(const QVariant& value, const QLocale&) const
{
    if (value.type() == QVariant::String) {
        return value.toString();
    }
    else if (value.canConvert<Property*>()) {
        const Property* prop = qvariant_cast<Property*>(value);
        //return propertyValueText(prop);
        const char* propTypeName = prop ? prop->dynTypeName() : "";
        if (propTypeName == PropertyBool::TypeName)
            return propertyValueText(static_cast<const PropertyBool*>(prop));

        if (propTypeName == PropertyInt::TypeName)
            return propertyValueText(static_cast<const PropertyInt*>(prop));

        if (propTypeName == PropertyDouble::TypeName)
            return propertyValueText(static_cast<const PropertyDouble*>(prop));

        if (propTypeName == PropertyCheckState::TypeName)
            return propertyValueText(static_cast<const PropertyCheckState*>(prop));

        if (propTypeName == PropertyQByteArray::TypeName)
            return propertyValueText(static_cast<const PropertyQByteArray*>(prop));

        if (propTypeName == PropertyQString::TypeName)
            return propertyValueText(static_cast<const PropertyQString*>(prop));

        if (propTypeName == PropertyQDateTime::TypeName)
            return propertyValueText(static_cast<const PropertyQDateTime*>(prop));

        if (propTypeName == PropertyOccColor::TypeName)
            return propertyValueText(static_cast<const PropertyOccColor*>(prop));

        if (propTypeName == PropertyOccPnt::TypeName)
            return propertyValueText(static_cast<const PropertyOccPnt*>(prop));

        if (propTypeName == PropertyOccTrsf::TypeName)
            return propertyValueText(static_cast<const PropertyOccTrsf*>(prop));

        if (propTypeName == PropertyEnumeration::TypeName)
            return propertyValueText(static_cast<const PropertyEnumeration*>(prop));

        if (propTypeName == BasePropertyQuantity::TypeName) {
            auto qtyProp = static_cast<const BasePropertyQuantity*>(prop);
            auto itFound = m_mapPropUnitTr.find(qtyProp);
            if (itFound != m_mapPropUnitTr.cend())
                return propertyValueText(qtyProp, itFound->second);
            else
                return propertyValueText(qtyProp);
        }

        return tr("ERROR no stringifier for property type '%1'").arg(propTypeName);
    }

    return QString();
}

QWidget* PropertyItemDelegate::createEditor(
        QWidget* parent, const QStyleOptionViewItem&, const QModelIndex& index) const
{
    if (index.column() == 0)
        return nullptr;

    auto property = qvariant_cast<Property*>(index.data());
    if (!property || property->isUserReadOnly())
        return nullptr;

    if (this->editorFactory())
        return this->editorFactory()->createEditor(property, parent);

    return nullptr;
}

void PropertyItemDelegate::setModelData(QWidget*, QAbstractItemModel*, const QModelIndex&) const
{
    // Disable default behavior that sets item data(property is changed directly)
}

QSize PropertyItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const QSize baseSize = QStyledItemDelegate::sizeHint(option, index);
    if (index.data(Qt::SizeHintRole).isNull())
        return QSize(baseSize.width(), m_rowHeightFactor * baseSize.height());

    return baseSize;
}

} // namespace Mayo
