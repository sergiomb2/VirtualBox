/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationModel class implementation.
 */

/*
 * Copyright (C) 2021-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QSet>

/* GUI includes: */
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UINotificationModel.h"
#include "UINotificationObject.h"

/* Other VBox includes: */
#include "iprt/assert.h"


UINotificationModel::UINotificationModel(QObject *pParent)
    : QObject(pParent)
{
    prepare();
}

UINotificationModel::~UINotificationModel()
{
    cleanup();
}

QUuid UINotificationModel::appendObject(UINotificationObject *pObject)
{
    /* [Re]generate ID until unique: */
    QUuid uId = QUuid::createUuid();
    while (m_ids.contains(uId))
        uId = QUuid::createUuid();
    /* Append ID and object: */
    m_ids << uId;
    m_objects[uId] = pObject;
    /* Connect object close signal: */
    connect(pObject, &UINotificationObject::sigAboutToClose,
            this, &UINotificationModel::sltHandleAboutToClose);
    /* Notify listeners: */
    emit sigItemAdded(uId);
    /* Handle object: */
    pObject->handle();
    /* Return ID: */
    return uId;
}

void UINotificationModel::revokeObject(const QUuid &uId)
{
    /* Remove id first of all: */
    m_ids.removeAll(uId);
    /* Notify listeners before object is deleted: */
    emit sigItemRemoved(uId);
    /* Delete object itself finally: */
    delete m_objects.take(uId);
}

bool UINotificationModel::hasObject(const QUuid &uId) const
{
    return m_objects.contains(uId);
}

void UINotificationModel::revokeFinishedObjects()
{
    /* Check whether there are done objects: */
    foreach (const QUuid &uId, m_ids)
    {
        UINotificationObject *pObject = m_objects.value(uId);
        AssertPtrReturnVoid(pObject);
        if (pObject->isDone())
            revokeObject(uId);
    }
}

QList<QUuid> UINotificationModel::ids() const
{
    return m_ids;
}

UINotificationObject *UINotificationModel::objectById(const QUuid &uId)
{
    return m_objects.value(uId);
}

void UINotificationModel::sltHandleAboutToClose(bool fDismiss)
{
    /* Determine sender: */
    UINotificationObject *pSender = qobject_cast<UINotificationObject*>(sender());
    AssertPtrReturnVoid(pSender);

    /* Dismiss message if requested: */
    if (fDismiss && !pSender->internalName().isEmpty())
    {
        QStringList suppressedMessages = gEDataManager->suppressedMessages();
        if (!suppressedMessages.contains(pSender->internalName()))
        {
            suppressedMessages.push_back(pSender->internalName());
            gEDataManager->setSuppressedMessages(suppressedMessages);
        }
    }

    /* Revoke it from internal storage: */
    const QUuid uId = m_objects.key(pSender);
    AssertReturnVoid(!uId.isNull());
    revokeObject(uId);
}

void UINotificationModel::sltDetachCOM()
{
    cleanup();
}

void UINotificationModel::prepare()
{
    connect(&uiCommon(), &UICommon::sigAskToDetachCOM,
            this, &UINotificationModel::sltDetachCOM);
}

void UINotificationModel::cleanup()
{
    /* Wipe out all the objects: */
    foreach (const QUuid &uId, m_ids)
        delete m_objects.value(uId);
    m_objects.clear();
    m_ids.clear();
}
