﻿/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsModel class implementation.
 */

/*
 * Copyright (C) 2012-2025 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QSignalTransition>
#include <QState>
#include <QStateMachine>
#include <QTimer>

/* GUI includes: */
#include "UIActionPoolManager.h"
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UILoggingDefs.h"
#include "UIToolsModel.h"
#include "UITranslationEventListener.h"

/* Other VBox includes: */
#include "iprt/assert.h"

/* Type defs: */
typedef QSet<QString> UIStringSet;


/** QPropertyAnimation extension used as tool-item animation. */
class UIToolItemAnimation : public QPropertyAnimation
{
    Q_OBJECT;

public:

    /** Constructs tool-item animation passing @a pParent to the base-class.
      * @param  pTarget       Brings the object animation alters property for.
      * @param  propertyName  Brings the name of property inside the @a pTarget.
      * @param  fForward      Brings whether animation goes to iValue or from it. */
    UIToolItemAnimation(QObject *pTarget, const QByteArray &propertyName, QObject *pParent, bool fForward);
};


/** QObject extension used as animation engine object. */
class UIToolsAnimationEngine : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about Home type selected. */
    void sigSelectedHome();
    /** Notifies about Machines type selected. */
    void sigSelectedMach();
    /** Notifies about Managers type selected. */
    void sigSelectedMana();

public:

    /** Animation states. */
    enum State
    {
        State_Home,
        State_Machines,
        State_Managers,
        State_LeavingHome,
        State_LeavingMachines,
        State_LeavingManagers
    };

    /** Constructs animation engine passing @a pParent to the base-class. */
    UIToolsAnimationEngine(UIToolsModel *pParent);

    /** Performs animation engine initialization. */
    void init();

    /** Returns animation state. */
    State state() const { return m_enmState; }

private slots:

    /** Handles signal about tool @a enmType change. */
    void sltHandleSelectionChanged(UIToolType enmType);

    /** Handles animation start. */
    void sltHandleAnimationStarted();
    /** Handles animation finish. */
    void sltHandleAnimationFinished();

private:

    /** Prepares everything. */
    void prepare();
    /** Prepares machine. */
    void prepareMachine();
    /** Prepares Home state. */
    void prepareStateHome();
    /** Prepares Machines state. */
    void prepareStateMachines();
    /** Prepares Managers state. */
    void prepareStateManagers();
    /** Prepares connections. */
    void prepareConnections();

    /** Holds the parent model reference. */
    UIToolsModel *m_pParent;

    /** Holds the state-machine instance. */
    QStateMachine *m_pMachine;

    /** Holds the Home state instance. */
    QState *m_pStateHome;
    /** Holds the Machines state instance. */
    QState *m_pStateMach;
    /** Holds the Managers state instance. */
    QState *m_pStateMana;

    /** Holds the animation state. */
    State  m_enmState;
};


/*********************************************************************************************************************************
*   Class UIToolItemAnimation implementation.                                                                                    *
*********************************************************************************************************************************/

UIToolItemAnimation::UIToolItemAnimation(QObject *pTarget, const QByteArray &propertyName, QObject *pParent, bool fForward)
    : QPropertyAnimation(pTarget, propertyName, pParent)
{
    setEasingCurve(QEasingCurve(QEasingCurve::OutQuart));
    setStartValue(fForward ? 0 : 100);
    setEndValue(fForward ? 100 : 0);
    setDuration(1000);
}


/*********************************************************************************************************************************
*   Class UIToolsAnimationEngine implementation.                                                                                 *
*********************************************************************************************************************************/

UIToolsAnimationEngine::UIToolsAnimationEngine(UIToolsModel *pParent)
    : QObject(pParent)
    , m_pParent(pParent)
    , m_pMachine(0)
    , m_pStateHome(0)
    , m_pStateMach(0)
    , m_pStateMana(0)
    , m_enmState(State_Home)
{
    prepare();
}

void UIToolsAnimationEngine::init()
{
    /* Define initial animation state: */
    QState *pInitialState = m_pStateHome;
    switch (m_pParent->toolsType(UIToolClass_Global))
    {
        case UIToolType_Machines: pInitialState = m_pStateMach; break;
        case UIToolType_Managers: pInitialState = m_pStateMana; break;
        default: break;
    }
    m_pMachine->setInitialState(pInitialState);
    m_pMachine->start();
}

void UIToolsAnimationEngine::sltHandleSelectionChanged(UIToolType enmType)
{
    /* Determine changed tool class: */
    const UIToolClass enmClass = UIToolStuff::castTypeToClass(enmType);

    /* Watch for changes in the Global class only: */
    if (enmClass == UIToolClass_Global)
    {
        /* Notify about certain item-types selected: */
        switch (enmType)
        {
            case UIToolType_Home: emit sigSelectedHome(); break;
            case UIToolType_Machines: emit sigSelectedMach(); break;
            case UIToolType_Managers: emit sigSelectedMana(); break;
            default: break;
        }
    }
}

void UIToolsAnimationEngine::sltHandleAnimationStarted()
{
    /* Recalculate effective state: */
    m_enmState = State_LeavingHome;
    if (m_pMachine->configuration().contains(m_pStateMach))
        m_enmState = State_LeavingMachines;
    else if (m_pMachine->configuration().contains(m_pStateMana))
        m_enmState = State_LeavingManagers;
}

void UIToolsAnimationEngine::sltHandleAnimationFinished()
{
    /* Recalculate effective state: */
    m_enmState = State_Home;
    if (m_pMachine->configuration().contains(m_pStateMach))
        m_enmState = State_Machines;
    else if (m_pMachine->configuration().contains(m_pStateMana))
        m_enmState = State_Managers;

    /* Update layout one more final time: */
    m_pParent->updateLayout();
}

void UIToolsAnimationEngine::prepare()
{
    prepareMachine();
    prepareConnections();
}

void UIToolsAnimationEngine::prepareMachine()
{
    /* Prepare animation machine: */
    m_pMachine = new QStateMachine(this);
    if (m_pMachine)
    {
        /* Prepare states: */
        m_pStateHome = new QState(m_pMachine);
        m_pStateMach = new QState(m_pMachine);
        m_pStateMana = new QState(m_pMachine);
        prepareStateHome();
        prepareStateMachines();
        prepareStateManagers();
    }
}

void UIToolsAnimationEngine::prepareStateHome()
{
    /* Configure Home state: */
    if (m_pStateHome)
    {
        m_pStateHome->assignProperty(m_pParent, "animationProgressMachines", 0);
        m_pStateHome->assignProperty(m_pParent, "animationProgressManagers", 0);

        /* Add Home=>Machines state transition: */
        QSignalTransition *pHomeToMachines = m_pStateHome->addTransition(this, SIGNAL(sigSelectedMach()), m_pStateMach);
        if (pHomeToMachines)
        {
            /* Create animation for animationProgressMachines: */
            UIToolItemAnimation *pAnmHomeMach = new UIToolItemAnimation(m_pParent, "animationProgressMachines", this, true);
            pHomeToMachines->addAnimation(pAnmHomeMach);
        }

        /* Add Home=>Managers state transition: */
        QSignalTransition *pHomeToManagers = m_pStateHome->addTransition(this, SIGNAL(sigSelectedMana()), m_pStateMana);
        if (pHomeToManagers)
        {
            /* Create animation for animationProgressManagers: */
            UIToolItemAnimation *pAnmHomeMana = new UIToolItemAnimation(m_pParent, "animationProgressManagers", this, true);
            pHomeToManagers->addAnimation(pAnmHomeMana);
        }
    }
}

void UIToolsAnimationEngine::prepareStateMachines()
{
    /* Configure Machines state: */
    if (m_pStateMach)
    {
        m_pStateMach->assignProperty(m_pParent, "animationProgressMachines", 100);
        m_pStateMach->assignProperty(m_pParent, "animationProgressManagers", 0);

        /* Add Machines=>Home state transition: */
        QSignalTransition *pMachinesToHome = m_pStateMach->addTransition(this, SIGNAL(sigSelectedHome()), m_pStateHome);
        if (pMachinesToHome)
        {
            /* Create animation for animationProgressMachines: */
            UIToolItemAnimation *pAnmMachHome = new UIToolItemAnimation(m_pParent, "animationProgressMachines", this, false);
            pMachinesToHome->addAnimation(pAnmMachHome);
        }

        /* Add Machines=>Managers state transition: */
        QSignalTransition *pMachinesToManagers = m_pStateMach->addTransition(this, SIGNAL(sigSelectedMana()), m_pStateMana);
        if (pMachinesToManagers)
        {
            /* Create animation for animationProgressMachines: */
            UIToolItemAnimation *pAnmMachMana1 = new UIToolItemAnimation(m_pParent, "animationProgressMachines", this, false);
            pMachinesToManagers->addAnimation(pAnmMachMana1);

            /* Create animation for animationProgressManagers: */
            UIToolItemAnimation *pAnmMachMana2 = new UIToolItemAnimation(m_pParent, "animationProgressManagers", this, true);
            pMachinesToManagers->addAnimation(pAnmMachMana2);
        }
    }
}

void UIToolsAnimationEngine::prepareStateManagers()
{
    /* Configure Managers state: */
    if (m_pStateMana)
    {
        m_pStateMana->assignProperty(m_pParent, "animationProgressMachines", 0);
        m_pStateMana->assignProperty(m_pParent, "animationProgressManagers", 100);

        /* Add Managers=>Home state transition: */
        QSignalTransition *pManagersToHome = m_pStateMana->addTransition(this, SIGNAL(sigSelectedHome()), m_pStateHome);
        if (pManagersToHome)
        {
            /* Create animation for animationProgressManagers: */
            UIToolItemAnimation *pAnmManaHome = new UIToolItemAnimation(m_pParent, "animationProgressManagers", this, false);
            pManagersToHome->addAnimation(pAnmManaHome);
        }

        /* Add Managers=>Machines state transition: */
        QSignalTransition *pManagersToMachines = m_pStateMana->addTransition(this, SIGNAL(sigSelectedMach()), m_pStateMach);
        if (pManagersToMachines)
        {
            /* Create animation for animationProgressMachines: */
            UIToolItemAnimation *pAnmManaMach1 = new UIToolItemAnimation(m_pParent, "animationProgressMachines", this, true);
            pManagersToMachines->addAnimation(pAnmManaMach1);

            /* Create animation for animationProgressManagers: */
            UIToolItemAnimation *pAnmManaMach2 = new UIToolItemAnimation(m_pParent, "animationProgressManagers", this, false);
            pManagersToMachines->addAnimation(pAnmManaMach2);
        }
    }
}

void UIToolsAnimationEngine::prepareConnections()
{
    connect(m_pParent, &UIToolsModel::sigSelectionChanged, this, &UIToolsAnimationEngine::sltHandleSelectionChanged);
    connect(this, &UIToolsAnimationEngine::sigSelectedHome, this, &UIToolsAnimationEngine::sltHandleAnimationStarted);
    connect(this, &UIToolsAnimationEngine::sigSelectedMach, this, &UIToolsAnimationEngine::sltHandleAnimationStarted);
    connect(this, &UIToolsAnimationEngine::sigSelectedMana, this, &UIToolsAnimationEngine::sltHandleAnimationStarted);
    connect(m_pStateHome, &QState::propertiesAssigned, this, &UIToolsAnimationEngine::sltHandleAnimationFinished);
    connect(m_pStateMach, &QState::propertiesAssigned, this, &UIToolsAnimationEngine::sltHandleAnimationFinished);
    connect(m_pStateMana, &QState::propertiesAssigned, this, &UIToolsAnimationEngine::sltHandleAnimationFinished);
}


/*********************************************************************************************************************************
*   Class UIToolsModel implementation.                                                                                           *
*********************************************************************************************************************************/

UIToolsModel::UIToolsModel(QObject *pParent, UIActionPool *pActionPool)
    : QObject(pParent)
    , m_pActionPool(pActionPool)
    , m_pView(0)
    , m_pScene(0)
    , m_fItemsEnabled(true)
    , m_fShowItemNames(gEDataManager->isToolTextVisible())
    , m_pAnimationEngine(0)
    , m_iOverallShiftMachines(0)
    , m_iOverallShiftManagers(0)
    , m_iAnimatedShiftMachines(0)
    , m_iAnimatedShiftManagers(0)
{
    prepare();
}

UIToolsModel::~UIToolsModel()
{
    cleanup();
}

void UIToolsModel::init()
{
    /* Update linked values: */
    updateLayout();
    sltItemMinimumWidthHintChanged();
    sltItemMinimumHeightHintChanged();

    /* Load current items: */
    loadCurrentItems();

    /* Init animation engine: */
    if (m_pAnimationEngine)
        m_pAnimationEngine->init();
}

QGraphicsScene *UIToolsModel::scene() const
{
    return m_pScene;
}

QPaintDevice *UIToolsModel::paintDevice() const
{
    if (scene() && !scene()->views().isEmpty())
        return scene()->views().first();
    return 0;
}

QGraphicsItem *UIToolsModel::itemAt(const QPointF &position, const QTransform &deviceTransform /* = QTransform() */) const
{
    return scene() ? scene()->itemAt(position, deviceTransform) : 0;
}

UIToolsView *UIToolsModel::view() const
{
    return m_pView;
}

void UIToolsModel::setView(UIToolsView *pView)
{
    m_pView = pView;
}

void UIToolsModel::setToolsType(UIToolType enmType)
{
    const UIToolClass enmClass = UIToolStuff::castTypeToClass(enmType);
    if (!currentItem(enmClass) || currentItem(enmClass)->itemType() != enmType)
        setCurrentItem(item(enmType));
}

UIToolType UIToolsModel::toolsType(UIToolClass enmClass) const
{
    return currentItem(enmClass) ? currentItem(enmClass)->itemType() : UIToolType_Invalid;
}

void UIToolsModel::setItemsEnabled(bool fEnabled)
{
    if (m_fItemsEnabled != fEnabled)
    {
        m_fItemsEnabled = fEnabled;
        foreach (UIToolsItem *pItem, items())
            pItem->setEnabled(m_fItemsEnabled);
    }
}

bool UIToolsModel::isItemsEnabled() const
{
    return m_fItemsEnabled;
}

void UIToolsModel::setRestrictedToolTypes(UIToolClass enmClass, const QList<UIToolType> &types)
{
    if (m_mapRestrictedToolTypes.value(enmClass) != types)
    {
        m_mapRestrictedToolTypes[enmClass] = types;
        foreach (UIToolsItem *pItem, items())
        {
            if (pItem->itemClass() != enmClass)
                continue;
            const bool fRestricted = m_mapRestrictedToolTypes.value(enmClass).contains(pItem->itemType());
            pItem->setHiddenByReason(fRestricted, UIToolsItem::HidingReason_Restricted);
        }

        /* Update linked values: */
        recalculateOverallShifts(enmClass);
        updateLayout();
        sltItemMinimumWidthHintChanged();
        sltItemMinimumHeightHintChanged();
    }
}

void UIToolsModel::setUnsuitableToolClass(UIToolClass enmClass, bool fUnsuitable)
{
    if (m_mapUnsuitableToolClasses.value(enmClass) != fUnsuitable)
    {
        m_mapUnsuitableToolClasses[enmClass] = fUnsuitable;
        foreach (UIToolsItem *pItem, items())
        {
            if (pItem->itemClass() != enmClass)
                continue;
            pItem->setHiddenByReason(fUnsuitable, UIToolsItem::HidingReason_Unsuitable);
        }

        /* Update linked values: */
        updateLayout();
        sltItemMinimumWidthHintChanged();
        sltItemMinimumHeightHintChanged();
    }
}

QList<UIToolType> UIToolsModel::restrictedToolTypes(UIToolClass enmClass) const
{
    return m_mapRestrictedToolTypes.value(enmClass);
}

QVariant UIToolsModel::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case ToolsModelData_Margin: return 0;
        case ToolsModelData_Spacing: return 1;

        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIToolsModel::setCurrentItem(UIToolsItem *pItem)
{
    /* Valid item passed? */
    if (pItem)
    {
        /* What's the item class? */
        const UIToolClass enmClass = pItem->itemClass();

        /* Is there something changed? */
        if (m_mapCurrentItems.value(enmClass) == pItem)
            return;

        /* Remember old current-item: */
        UIToolsItem *pOldCurrentItem = m_mapCurrentItems.value(enmClass);
        /* Set new item as current: */
        m_mapCurrentItems[enmClass] = pItem;

        /* Update old item (if any): */
        if (pOldCurrentItem)
            pOldCurrentItem->update();
        /* Update new item: */
        m_mapCurrentItems.value(enmClass)->update();

        /* Notify about selection change: */
        emit sigSelectionChanged(toolsType(enmClass));
    }
    /* Null item passed? */
    else
    {
        /* Is there something changed? */
        if (   !m_mapCurrentItems.value(UIToolClass_Global)
            && !m_mapCurrentItems.value(UIToolClass_Machine)
            && !m_mapCurrentItems.value(UIToolClass_Management))
            return;

        /* Clear all current items: */
        m_mapCurrentItems[UIToolClass_Global] = 0;
        m_mapCurrentItems[UIToolClass_Machine] = 0;
        m_mapCurrentItems[UIToolClass_Management] = 0;

        /* Notify about selection change: */
        emit sigSelectionChanged(UIToolType_Invalid);
    }
}

UIToolsItem *UIToolsModel::currentItem(UIToolClass enmClass) const
{
    return m_mapCurrentItems.value(enmClass);
}

QList<UIToolsItem*> UIToolsModel::items() const
{
    return m_items;
}

UIToolsItem *UIToolsModel::item(UIToolType enmType) const
{
    foreach (UIToolsItem *pItem, items())
        if (pItem->itemType() == enmType)
            return pItem;
    return 0;
}

bool UIToolsModel::showItemNames() const
{
    return m_fShowItemNames;
}

void UIToolsModel::updateLayout()
{
    /* Prepare variables: */
    const int iMargin = data(ToolsModelData_Margin).toInt();
    const int iSpacing = data(ToolsModelData_Spacing).toInt();
    const QSize viewportSize = scene()->views()[0]->viewport()->size();
    const int iViewportWidth = viewportSize.width();
    const int iViewportHeight = viewportSize.height();

    /* Start from above: */
    int iVerticalIndentGlobal = iMargin;
    int iVerticalIndentRest = iMargin;
    int iVerticalIndentSub = 0;

    /* Layout normal children: */
    foreach (UIToolsItem *pItem, items())
    {
        /* Skip aux children: */
        const UIToolClass enmClass = pItem->itemClass();
        if (enmClass == UIToolClass_Aux)
            continue;

        /* Make sure item visible: */
        if (!pItem->isVisible())
            continue;

        /* Acquire item properties: */
        const int iItemHeight = pItem->minimumHeightHint();

        /* Separate procedures for different classes: */
        switch (enmClass)
        {
            case UIToolClass_Global:
            {
                /* Set item position: */
                pItem->setPos(iMargin, iVerticalIndentGlobal);
                /* Set root-item size: */
                pItem->resize(iViewportWidth, iItemHeight);
                /* Make sure item is shown: */
                pItem->show();
                /* Advance vertical indent: */
                iVerticalIndentGlobal += (iItemHeight + iSpacing);
                iVerticalIndentRest += (iItemHeight + iSpacing);

                /* Do we have animation engine? */
                if (m_pAnimationEngine)
                {
                    /* Append some animated indentation after items of certain types: */
                    switch (pItem->itemType())
                    {
                        case UIToolType_Machines:
                        {
                            const double fRatio = (double)animationProgressMachines() / 50 /* make it from 0.0 to 2.0 */;
                            int iShift = 0;
                            if (m_pAnimationEngine->state() == UIToolsAnimationEngine::State_LeavingMachines)
                            {
                                /* Leaving animation uses fRatio from 2.0 to 1.0: */
                                iShift = overallShiftMachines() * (fRatio - 1);
                                iShift = qMax(iShift, 0);
                            }
                            else
                            {
                                /* Entering animation uses fRatio from 0.0 to 1.0: */
                                iShift = overallShiftMachines() * fRatio;
                                iShift = qMin(iShift, overallShiftMachines());
                            }
                            iVerticalIndentGlobal += iShift;
                            break;
                        }
                        case UIToolType_Managers:
                        {
                            const double fRatio = (double)animationProgressManagers() / 50 /* make it from 0.0 to 2.0 */;
                            int iShift = 0;
                            if (m_pAnimationEngine->state() == UIToolsAnimationEngine::State_LeavingManagers)
                            {
                                /* Leaving animation uses fRatio from 2.0 to 1.0: */
                                iShift = overallShiftManagers() * (fRatio - 1);
                                iShift = qMax(iShift, 0);
                            }
                            else
                            {
                                /* Entering animation uses fRatio from 0.0 to 1.0: */
                                iShift = overallShiftManagers() * fRatio;
                                iShift = qMin(iShift, overallShiftManagers());
                            }
                            iVerticalIndentGlobal += iShift;
                            break;
                        }
                        default:
                            break;
                    }
                }

                break;
            }
            case UIToolClass_Machine:
            case UIToolClass_Management:
            {
                /* Acquire item properties: */
                const int iItemHeight = pItem->minimumHeightHint();
                int iPos = iMargin;

                /* Do we have animation engine? */
                if (m_pAnimationEngine)
                {
                    /* Append some animated indentation to items of certain classes: */
                    switch (pItem->itemClass())
                    {
                        case UIToolClass_Machine:
                        {
                            const double fRatio = (double)animationProgressMachines() / 25 /* make it from 0.0 to 4.0 */;
                            iPos = iPos
                                 + (4.0 - fRatio) * iViewportWidth /* move item from off-screen space */
                                 + iVerticalIndentSub - fRatio / 4 * overallShiftMachines() /* desync item movement */;
                            break;
                        }
                        case UIToolClass_Management:
                        {
                            const double fRatio = (double)animationProgressManagers() / 25 /* make it from 0.0 to 4.0 */;
                            iPos = iPos
                                 + (4.0 - fRatio) * iViewportWidth /* move item from off-screen space */
                                 + iVerticalIndentSub - fRatio / 4 * overallShiftManagers() /* desync item movement */;
                            break;
                        }
                        default: break;
                    }

                    /* Restrain origin: */
                    iPos = qMax(iMargin, iPos);
                }

                /* Set item position: */
                pItem->setPos(iPos, iVerticalIndentRest);
                /* Set root-item size: */
                pItem->resize(iViewportWidth, iItemHeight);
                /* Make sure item is shown: */
                pItem->show();
                /* Advance vertical indent: */
                iVerticalIndentRest += (iItemHeight + iSpacing);
                iVerticalIndentSub += (iItemHeight + iSpacing);

                break;
            }
            default:
                break;
        }
    }

    /* Start from bottom: */
    int iVerticalIndentAux = iViewportHeight - iMargin;

    /* Layout aux children: */
    foreach (UIToolsItem *pItem, items())
    {
        /* Skip normal children: */
        if (pItem->itemClass() != UIToolClass_Aux)
            continue;

        /* Set item position: */
        pItem->setPos(iMargin, iVerticalIndentAux - pItem->minimumHeightHint());
        /* Set root-item size: */
        pItem->resize(iViewportWidth, pItem->minimumHeightHint());
        /* Make sure item is shown: */
        pItem->show();
        /* Decrease vertical indent: */
        iVerticalIndentAux -= (pItem->minimumHeightHint() + iSpacing);
    }
}

void UIToolsModel::sltItemMinimumWidthHintChanged()
{
    /* Prepare variables: */
    const int iMargin = data(ToolsModelData_Margin).toInt();

    /* Calculate maximum horizontal width: */
    int iMinimumWidthHint = 0;
    iMinimumWidthHint += 2 * iMargin;
    foreach (UIToolsItem *pItem, items())
        iMinimumWidthHint = qMax(iMinimumWidthHint, pItem->minimumWidthHint());

    /* Notify listeners: */
    emit sigItemMinimumWidthHintChanged(iMinimumWidthHint);
}

void UIToolsModel::sltItemMinimumHeightHintChanged()
{
    /* Prepare variables: */
    const int iMargin = data(ToolsModelData_Margin).toInt();
    const int iSpacing = data(ToolsModelData_Spacing).toInt();

    /* Calculate summary vertical height: */
    int iMinimumHeightHint = 0;
    iMinimumHeightHint += 2 * iMargin;
    foreach (UIToolsItem *pItem, items())
        if (pItem->isVisible())
            iMinimumHeightHint += (pItem->minimumHeightHint() + iSpacing);
    iMinimumHeightHint -= iSpacing;

    /* Notify listeners: */
    emit sigItemMinimumHeightHintChanged(iMinimumHeightHint);
}

bool UIToolsModel::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Process only scene events: */
    if (pWatched != scene())
        return QObject::eventFilter(pWatched, pEvent);

    /* Checking event-type: */
    switch (pEvent->type())
    {
        /* Mouse handler: */
        case QEvent::GraphicsSceneMouseRelease:
        {
            /* Acquire event: */
            QGraphicsSceneMouseEvent *pMouseEvent = static_cast<QGraphicsSceneMouseEvent*>(pEvent);
            /* Get item under mouse cursor: */
            QPointF scenePos = pMouseEvent->scenePos();
            if (QGraphicsItem *pItemUnderMouse = itemAt(scenePos))
            {
                /* Which item we just clicked? Is it enabled? */
                UIToolsItem *pClickedItem = qgraphicsitem_cast<UIToolsItem*>(pItemUnderMouse);
                if (pClickedItem && pClickedItem->isEnabled())
                {
                    /* Handle known item classes: */
                    switch (pClickedItem->itemClass())
                    {
                        case UIToolClass_Aux:
                        {
                            /* Handle known item types: */
                            switch (pClickedItem->itemType())
                            {
                                case UIToolType_Toggle:
                                {
                                    /* Toggle the button: */
                                    m_fShowItemNames = !m_fShowItemNames;
                                    /* Update geometry for all the items: */
                                    foreach (UIToolsItem *pItem, m_items)
                                        pItem->updateGeometry();
                                    /* Recalculate layout: */
                                    updateLayout();
                                    /* Save the change: */
                                    gEDataManager->setToolTextVisible(m_fShowItemNames);
                                    return true;
                                }
                                default:
                                    break;
                            }
                            break;
                        }
                        case UIToolClass_Global:
                        case UIToolClass_Machine:
                        case UIToolClass_Management:
                        {
                            /* Make clicked item the current one: */
                            if (pClickedItem->isEnabled())
                            {
                                setCurrentItem(pClickedItem);
                                return true;
                            }
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return QObject::eventFilter(pWatched, pEvent);
}

void UIToolsModel::sltHandleCommitData()
{
    /* Save current items first of all: */
    saveCurrentItems();
}

void UIToolsModel::sltRetranslateUI()
{
    foreach (UIToolsItem *pItem, m_items)
    {
        switch (pItem->itemType())
        {
            // Aux
            case UIToolType_Toggle:      pItem->setName(tr("Show text")); break;
            // Global
            case UIToolType_Home:        pItem->setName(tr("Home")); break;
            case UIToolType_Machines:    pItem->setName(tr("VMs")); break;
            case UIToolType_Managers:    pItem->setName(tr("Tools")); break;
            // Machine
            case UIToolType_Details:     pItem->setName(tr("Details")); break;
            case UIToolType_Snapshots:   pItem->setName(tr("Snapshots")); break;
            case UIToolType_Logs:        pItem->setName(tr("Logs")); break;
            case UIToolType_VMActivity:  pItem->setName(tr("Activity")); break;
            case UIToolType_FileManager: pItem->setName(tr("File Manager")); break;
            // Management
            case UIToolType_Extensions:  pItem->setName(tr("Extensions")); break;
            case UIToolType_Media:       pItem->setName(tr("Media")); break;
            case UIToolType_Network:     pItem->setName(tr("Network")); break;
            case UIToolType_Cloud:       pItem->setName(tr("Cloud")); break;
            case UIToolType_Activities:  pItem->setName(tr("Activities")); break;
            default: break;
        }
    }
}

void UIToolsModel::prepare()
{
    /* Prepare everything: */
    prepareScene();
    prepareItems();
    prepareAnimationEngine();
    prepareConnections();

    /* Apply language settings: */
    sltRetranslateUI();
}

void UIToolsModel::prepareScene()
{
    m_pScene = new QGraphicsScene(this);
    if (m_pScene)
        m_pScene->installEventFilter(this);
}

void UIToolsModel::prepareItems()
{
    /* Home: */
    m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/welcome_screen_24px.png",
                                                            ":/welcome_screen_24px.png"),
                               UIToolType_Home);

    /* Machines: */
    m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/machine_details_manager_24px.png",
                                                            ":/machine_details_manager_disabled_24px.png"),
                               UIToolType_Machines);

    {
        /* Details: */
        m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/machine_details_manager_24px.png",
                                                                ":/machine_details_manager_disabled_24px.png"),
                                   UIToolType_Details);

        /* Snapshots: */
        m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/snapshot_manager_24px.png",
                                                                ":/snapshot_manager_disabled_24px.png"),
                                   UIToolType_Snapshots);

        /* Logs: */
        m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/vm_show_logs_24px.png",
                                                                ":/vm_show_logs_disabled_24px.png"),
                                   UIToolType_Logs);

        /* Activity: */
        m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/performance_monitor_24px.png",
                                                                ":/performance_monitor_disabled_24px.png"),
                                   UIToolType_VMActivity);

        /* File Manager: */
        m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/file_manager_24px.png",
                                                                ":/file_manager_disabled_24px.png"),
                                   UIToolType_FileManager);
    }

    /* Management: */
    m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/extension_pack_manager_24px.png",
                                                            ":/extension_pack_manager_disabled_24px.png"),
                               UIToolType_Managers);

    {
        /* Extensions: */
        m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/extension_pack_manager_24px.png",
                                                                ":/extension_pack_manager_disabled_24px.png"),
                                   UIToolType_Extensions);

        /* Media: */
        m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/media_manager_24px.png",
                                                                ":/media_manager_disabled_24px.png"),
                                   UIToolType_Media);

        /* Network: */
        m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/host_iface_manager_24px.png",
                                                                ":/host_iface_manager_disabled_24px.png"),
                                   UIToolType_Network);

        /* Cloud: */
        m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/cloud_profile_manager_24px.png",
                                                                ":/cloud_profile_manager_disabled_24px.png"),
                                   UIToolType_Cloud);

        /* Activities: */
        m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/resources_monitor_24px.png",
                                                                ":/resources_monitor_disabled_24px.png"),
                                   UIToolType_Activities);
    }

    /* Toggle: */
    m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/tools_menu_24px.png",
                                                            ":/tools_menu_24px.png"),
                               UIToolType_Toggle);

    /* Calculate overall shifts: */
    recalculateOverallShifts();
}

void UIToolsModel::prepareAnimationEngine()
{
    m_pAnimationEngine = new UIToolsAnimationEngine(this);
}

void UIToolsModel::prepareConnections()
{
    /* UICommon connections: */
    connect(&uiCommon(), &UICommon::sigAskToCommitData,
            this, &UIToolsModel::sltHandleCommitData);

    /* Translation stuff: */
    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIToolsModel::sltRetranslateUI);
}

void UIToolsModel::loadCurrentItems()
{
    /* Load last tool types: */
    UIToolType enmTypeGlobal, enmTypeMachine, enmTypeManagment;
    gEDataManager->toolsPaneLastItemsChosen(enmTypeGlobal, enmTypeMachine, enmTypeManagment);
    LogRel2(("GUI: UIToolsModel: Restoring tool items as: Global=%d, Machine=%d, Management=%d\n",
             (int)enmTypeGlobal, (int)enmTypeMachine, (int)enmTypeManagment));
    UIToolsItem *pItem = 0;

    /* Global: */
    pItem = item(enmTypeGlobal);
    if (!pItem)
        pItem = item(UIToolType_Home);
    setCurrentItem(pItem);

    /* Machine: */
    pItem = item(enmTypeMachine);
    if (!pItem)
        pItem = item(UIToolType_Details);
    setCurrentItem(pItem);

    /* Management: */
    pItem = item(enmTypeManagment);
    if (!pItem)
        pItem = item(UIToolType_Extensions);
    setCurrentItem(pItem);
}

void UIToolsModel::saveCurrentItems()
{
    /* Load last tool types: */
    UIToolType enmTypeGlobal, enmTypeMachine, enmTypeManagment;
    gEDataManager->toolsPaneLastItemsChosen(enmTypeGlobal, enmTypeMachine, enmTypeManagment);

    /* Gather actual values, we are going to save them: */
    if (UIToolsItem *pItem = currentItem(UIToolClass_Global))
        enmTypeGlobal = pItem->itemType();
    if (UIToolsItem *pItem = currentItem(UIToolClass_Machine))
        enmTypeMachine = pItem->itemType();
    if (UIToolsItem *pItem = currentItem(UIToolClass_Management))
        enmTypeManagment = pItem->itemType();

    /* Save selected items data: */
    LogRel2(("GUI: UIToolsModel: Saving tool items as: Global=%d, Machine=%d, Management=%d\n",
             (int)enmTypeGlobal, (int)enmTypeMachine, (int)enmTypeManagment));
    gEDataManager->setToolsPaneLastItemsChosen(enmTypeGlobal, enmTypeMachine, enmTypeManagment);
}

void UIToolsModel::cleanupItems()
{
    foreach (UIToolsItem *pItem, m_items)
        delete pItem;
    m_items.clear();
}

void UIToolsModel::cleanupScene()
{
    delete m_pScene;
    m_pScene = 0;
}

void UIToolsModel::cleanup()
{
    /* Cleanup everything: */
    cleanupItems();
    cleanupScene();
}

void UIToolsModel::recalculateOverallShifts(UIToolClass enmClass /* = UIToolClass_Invalid */)
{
    /* Some geometry stuff: */
    const int iSpacing = data(UIToolsModel::ToolsModelData_Spacing).toInt();

    /* Recalculate minimum vertical hint for items of Machine class: */
    if (   enmClass == UIToolClass_Invalid
        || enmClass == UIToolClass_Machine)
    {
        const QList<UIToolType> types = restrictedToolTypes(UIToolClass_Machine);
        m_iOverallShiftMachines = 0;
        AssertReturnVoid(!items().isEmpty());
        foreach (UIToolsItem *pItem, items())
            if (   !types.contains(pItem->itemType())
                && pItem->itemClass() == UIToolClass_Machine)
                m_iOverallShiftMachines += pItem->minimumHeightHint() + iSpacing;
        if (m_iOverallShiftMachines)
            m_iOverallShiftMachines -= iSpacing;
    }

    /* Recalculate minimum vertical hint for items of Management class: */
    if (   enmClass == UIToolClass_Invalid
        || enmClass == UIToolClass_Management)
    {
        const QList<UIToolType> types = restrictedToolTypes(UIToolClass_Management);
        m_iOverallShiftManagers = 0;
        AssertReturnVoid(!items().isEmpty());
        foreach (UIToolsItem *pItem, items())
            if (   !types.contains(pItem->itemType())
                && pItem->itemClass() == UIToolClass_Management)
                m_iOverallShiftManagers += pItem->minimumHeightHint() + iSpacing;
        if (m_iOverallShiftManagers)
            m_iOverallShiftManagers -= iSpacing;
    }
}

void UIToolsModel::setAnimationProgressMachines(int iAnimatedValue)
{
    m_iAnimatedShiftMachines = iAnimatedValue;
    updateLayout();
}

void UIToolsModel::setAnimationProgressManagers(int iAnimatedValue)
{
    m_iAnimatedShiftManagers = iAnimatedValue;
    updateLayout();
}


#include "UIToolsModel.moc"
