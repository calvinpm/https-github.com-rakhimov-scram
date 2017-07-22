/*
 * Copyright (C) 2017 Olzhas Rakhimov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "eventdialog.h"

#include <limits>

#include <QDoubleValidator>
#include <QListView>
#include <QObject>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QStatusBar>

#include "src/element.h"
#include "src/event.h"
#include "src/expression/constant.h"
#include "src/expression/exponential.h"
#include "src/ext/bits.h"
#include "src/ext/variant.h"

#include "guiassert.h"

namespace scram {
namespace gui {

QString EventDialog::redBackground(QStringLiteral("background : red;"));
QString EventDialog::yellowBackground(QStringLiteral("background : yellow;"));

#define OVERLOAD(type, name, ...)                                              \
    static_cast<void (type::*)(__VA_ARGS__)>(&type::name)

EventDialog::EventDialog(mef::Model *model, QWidget *parent)
    : QDialog(parent), m_model(model), m_errorBar(new QStatusBar(this))
{
    static QRegularExpressionValidator nameValidator(
        QRegularExpression(QStringLiteral(R"([[:alpha:]]\w*(-\w+)*)")));
    static QDoubleValidator nonNegativeValidator(
        0, std::numeric_limits<double>::max(), 1000);
    static QDoubleValidator probabilityValidator(0, 1, 1000);

    setupUi(this);
    gridLayout->addWidget(m_errorBar, gridLayout->rowCount(), 0,
                          gridLayout->rowCount(), gridLayout->columnCount());

    nameLine->setValidator(&nameValidator);
    constantValue->setValidator(&probabilityValidator);
    exponentialRate->setValidator(&nonNegativeValidator);
    addArgLine->setValidator(&nameValidator);

    connect(typeBox, OVERLOAD(QComboBox, currentIndexChanged, int),
            [this](int index) {
                switch (static_cast<EventType>(1 << index)) {
                case HouseEvent:
                    GUI_ASSERT(typeBox->currentText() == tr("House event"), );
                    stackedWidgetType->setCurrentWidget(tabBoolean);
                    break;
                case BasicEvent:
                case Undeveloped:
                case Conditional:
                    stackedWidgetType->setCurrentWidget(tabExpression);
                    break;
                case Gate:
                    stackedWidgetType->setCurrentWidget(tabFormula);
                    break;
                default:
                    GUI_ASSERT(false, );
                }
                validate();
            });
    connect(expressionType, OVERLOAD(QComboBox, currentIndexChanged, int), this,
            &EventDialog::validate);
    connect(expressionBox, &QGroupBox::toggled, this, &EventDialog::validate);
    connectLineEdits({nameLine, constantValue, exponentialRate});
    connect(connectiveBox, OVERLOAD(QComboBox, currentIndexChanged, int),
            [this](int index) {
                voteNumberBox->setEnabled(index == mef::kVote);
                validate();
            });
    connect(this, &EventDialog::formulaArgsChanged, [this] {
        int numArgs = argsList->count();
        int newMax = numArgs > 2 ? (numArgs - 1) : 2;
        if (voteNumberBox->value() > newMax)
            voteNumberBox->setValue(newMax);
        voteNumberBox->setMaximum(newMax);
        validate();
    });
    connect(addArgLine, &QLineEdit::returnPressed, this, [this] {
                QString name = addArgLine->text();
                addArgLine->setStyleSheet(yellowBackground);
                if (hasFormulaArg(name)) {
                    m_errorBar->showMessage(
                        tr("The argument '%1' is already in formula.")
                            .arg(name));
                    return;
                }
                if (name == nameLine->text()) {
                    m_errorBar->showMessage(
                        tr("The argument '%1' would introduce a self-cycle.")
                            .arg(name));
                    return;
                }
                addArgLine->setStyleSheet({});
                /// @todo Check for the cycle.
                argsList->addItem(name);
                emit formulaArgsChanged();
            });
    connect(addArgLine, &QLineEdit::textChanged,
            [this] { addArgLine->setStyleSheet({}); });
    stealTopFocus(addArgLine);

    // Ensure proper defaults.
    GUI_ASSERT(typeBox->currentIndex() == 0, );
    GUI_ASSERT(stackedWidgetType->currentIndex() == 0, );
    GUI_ASSERT(expressionType->currentIndex() == 0, );
    GUI_ASSERT(stackedWidgetExpressionData->currentIndex() == 0, );

    // Validation triggers.
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    GUI_ASSERT(okButton, );
    okButton->setEnabled(false);
    connect(this, &EventDialog::validated, okButton, &QPushButton::setEnabled);
}

bool EventDialog::hasFormulaArg(const QString &name)
{
    for (int i = 0; i < argsList->count(); ++i) {
        if (argsList->item(i)->data(Qt::DisplayRole) == name)
            return true;
    }
    return false;
}

void EventDialog::setupData(const model::Element &element)
{
    m_initName = element.id();
    nameLine->setText(m_initName);
    labelText->setPlainText(element.label());
    nameLine->setEnabled(false);
}

void EventDialog::setupData(const model::HouseEvent &element)
{
    setupData(static_cast<const model::Element &>(element));
    typeBox->setEnabled(false); ///< @todo Type change.
    typeBox->setCurrentIndex(ext::one_bit_index(HouseEvent));
    stateBox->setCurrentIndex(element.state());
}

void EventDialog::setupData(const model::BasicEvent &element)
{
    setupData(static_cast<const model::Element &>(element));
    /// @todo Type change.
    static_cast<QListView *>(typeBox->view())
        ->setRowHidden(ext::one_bit_index(HouseEvent), true);
    static_cast<QListView *>(typeBox->view())
        ->setRowHidden(ext::one_bit_index(Gate), true);
    typeBox->setCurrentIndex(ext::one_bit_index(BasicEvent) + element.flavor());
    auto &basicEvent = static_cast<const mef::BasicEvent &>(*element.data());
    if (basicEvent.HasExpression()) {
        expressionBox->setChecked(true);
        if (auto *constExpr = dynamic_cast<mef::ConstantExpression *>(
                &basicEvent.expression())) {
            expressionType->setCurrentIndex(0);
            constantValue->setText(QString::number(constExpr->value()));
        } else {
            auto *exponentialExpr = dynamic_cast<mef::Exponential *>(
                &basicEvent.expression());
            GUI_ASSERT(exponentialExpr, );
            expressionType->setCurrentIndex(1);
            exponentialRate->setText(
                QString::number(exponentialExpr->args().front()->value()));
        }
    } else {
        expressionBox->setChecked(false);
    }
}

void EventDialog::setupData(const model::Gate &element)
{
    setupData(static_cast<const model::Element &>(element));
    typeBox->setEnabled(false); ///< @todo Type change.
    typeBox->setCurrentIndex(ext::one_bit_index(Gate));
    connectiveBox->setCurrentIndex(element.type());
    connectiveBox->setEnabled(false); ///< @todo Connective change.
    if (element.type() == mef::kVote)
        voteNumberBox->setValue(element.voteNumber());
    voteNumberBox->setEnabled(false); ///< @todo Vote number change.
    addArgLine->setEnabled(false); ///< @todo Gate arg addition.
    argsList->setEnabled(false); ///< @todo Gate arg manipulation.
    for (const mef::Formula::EventArg &arg : element.args())
        argsList->addItem(
            QString::fromStdString(ext::as<const mef::Event *>(arg)->id()));
    emit formulaArgsChanged();
}

std::unique_ptr<mef::Expression> EventDialog::expression() const
{
    GUI_ASSERT(tabExpression->isHidden() == false, nullptr);
    if (expressionBox->isChecked() == false)
        return nullptr;
    switch (stackedWidgetExpressionData->currentIndex()) {
    case 0:
        GUI_ASSERT(constantValue->hasAcceptableInput(), nullptr);
        return std::make_unique<mef::ConstantExpression>(
            constantValue->text().toDouble());
    case 1: {
        GUI_ASSERT(exponentialRate->hasAcceptableInput(), nullptr);
        std::unique_ptr<mef::Expression> rate(
            new mef::ConstantExpression(exponentialRate->text().toDouble()));
        auto *rate_arg = rate.get();
        m_model->Add(std::move(rate));
        return std::make_unique<mef::Exponential>(
            rate_arg, m_model->mission_time().get());
    }
    default:
        GUI_ASSERT(false && "unexpected expression", nullptr);
    }
}

void EventDialog::validate()
{
    m_errorBar->clearMessage();
    emit validated(false);

    if (nameLine->hasAcceptableInput() == false)
        return;
    QString name = nameLine->text();
    nameLine->setStyleSheet(yellowBackground);
    try {
        if (name != m_initName) {
            m_model->GetEvent(name.toStdString(), "");
            m_errorBar->showMessage(
                tr("The event with name '%1' already exists.").arg(name));
            return;
        }
    } catch (std::out_of_range &) {
    }
    if (!tabFormula->isHidden() && hasFormulaArg(name)) {
        m_errorBar->showMessage(
            tr("Name '%1' would introduce a self-cycle.").arg(name));
        return;
    }
    nameLine->setStyleSheet({});

    if (!tabExpression->isHidden() && expressionBox->isChecked()) {
        switch (stackedWidgetExpressionData->currentIndex()) {
        case 0:
            if (constantValue->hasAcceptableInput() == false)
                return;
            break;
        case 1:
            if (exponentialRate->hasAcceptableInput() == false)
                return;
            break;
        default:
            GUI_ASSERT(false && "unexpected expression", );
        }
    }

    if (!tabFormula->isHidden()) {
        int numArgs = argsList->count();
        switch (static_cast<mef::Operator>(connectiveBox->currentIndex())) {
        case mef::kNot:
        case mef::kNull:
            if (numArgs != 1) {
                m_errorBar->showMessage(
                    tr("%1 connective requires a single argument.")
                        .arg(connectiveBox->currentText()));
                return;
            }
            break;
        case mef::kAnd:
        case mef::kOr:
        case mef::kNand:
        case mef::kNor:
            if (numArgs < 2) {
                m_errorBar->showMessage(
                    tr("%1 connective requires 2 or more arguments.")
                        .arg(connectiveBox->currentText()));
                return;
            }
            break;
        case mef::kXor:
            if (numArgs != 2) {
                m_errorBar->showMessage(
                    tr("%1 connective requires exactly 2 arguments.")
                        .arg(connectiveBox->currentText()));
                return;
            }
            break;
        case mef::kVote:
            if (numArgs <= voteNumberBox->value()) {
                m_errorBar->showMessage(
                    tr("%1 connective requires at-least %2 arguments.")
                        .arg(connectiveBox->currentText(),
                             QString::number(voteNumberBox->value() + 1)));
                return;
            }
            break;
        default:
            GUI_ASSERT(false && "unexpected connective", );
        }
    }

    emit validated(true);
}

void EventDialog::connectLineEdits(std::initializer_list<QLineEdit *> lineEdits)
{
    for (QLineEdit *lineEdit : lineEdits) {
        lineEdit->setStyleSheet(redBackground);
        connect(lineEdit, &QLineEdit::textChanged, [this, lineEdit] {
            if (lineEdit->hasAcceptableInput())
                lineEdit->setStyleSheet({});
            else
                lineEdit->setStyleSheet(redBackground);
            validate();
        });
    }
}

void EventDialog::stealTopFocus(QLineEdit *lineEdit)
{
    struct FocusGrabber : public QObject {
        FocusGrabber(QObject *parent, QPushButton *okButton)
            : QObject(parent), m_ok(okButton)
        {
        }
        bool eventFilter(QObject *object, QEvent *event) override
        {
            if (event->type() == QEvent::FocusIn) {
                m_ok->setDefault(false);
                m_ok->setAutoDefault(false);
            } else if (event->type() == QEvent::FocusOut) {
                m_ok->setDefault(true);
                m_ok->setAutoDefault(true);
            }
            return QObject::eventFilter(object, event);
        }
        QPushButton *m_ok;
    };
    lineEdit->installEventFilter(
        new FocusGrabber(lineEdit, buttonBox->button(QDialogButtonBox::Ok)));
}

} // namespace gui
} // namespace scram
