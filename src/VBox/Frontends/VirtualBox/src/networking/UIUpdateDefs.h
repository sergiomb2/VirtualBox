/* $Id$ */
/** @file
 * VBox Qt GUI - Update routine related declarations.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_networking_UIUpdateDefs_h
#define FEQT_INCLUDED_SRC_networking_UIUpdateDefs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QDate>

/* GUI includes: */
#include "UILibraryDefs.h"
#include "UIVersion.h"

/* COM includes: */
#include "COMEnums.h"


/** Structure to store retranslated reminder values. */
struct VBoxUpdateDay
{
    VBoxUpdateDay(const QString &strVal, const QString &strKey)
        : val(strVal), key(strKey) {}

    bool operator==(const VBoxUpdateDay &other) const { return val == other.val || key == other.key; }

    QString val;
    QString key;
};
typedef QList<VBoxUpdateDay> VBoxUpdateDayList;


/** Class used to encode/decode update data. */
class SHARED_LIBRARY_STUFF VBoxUpdateData
{
public:

    /** Period types. */
    enum PeriodType
    {
        PeriodNever     = -2,
        PeriodUndefined = -1,
        Period1Day      =  0,
        Period2Days     =  1,
        Period3Days     =  2,
        Period4Days     =  3,
        Period5Days     =  4,
        Period6Days     =  5,
        Period1Week     =  6,
        Period2Weeks    =  7,
        Period3Weeks    =  8,
        Period1Month    =  9
    };

    /** Populates a set of update options. */
    static void populate();
    /** Returns a list of update options. */
    static QStringList list();

    /** Constructs update description on the basis of passed @a strData. */
    VBoxUpdateData(const QString &strData = QString());
    /** Constructs update description on the basis of passed @a enmPeriodIndex and @a enmUpdateChannel. */
    VBoxUpdateData(PeriodType enmPeriodIndex, KUpdateChannel enmUpdateChannel);

    /** Returns whether there is no need to check. */
    bool isNoNeedToCheck() const;
    /** Returns whether there is really need to check. */
    bool isNeedToCheck() const;
    /** Returns update data. */
    QString data() const;
    /** Returns period index. */
    PeriodType periodIndex() const;
    /** Returns update date. */
    QString date() const;
    /** Returns internal update date. */
    QDate internalDate() const;
    /** Returns update channel. */
    KUpdateChannel updateChannel() const;
    /** Returns update channel name. */
    QString updateChannelName() const;
    /** Returns version. */
    UIVersion version() const;

    /** Returns whether this item equals to @a another one. */
    bool isEqual(const VBoxUpdateData &another) const;
    /** Returns whether this item equals to @a another one. */
    bool operator==(const VBoxUpdateData &another) const;
    /** Returns whether this item isn't equal to @a another one. */
    bool operator!=(const VBoxUpdateData &another) const;

    /** Converts passed @a enmUpdateChannel to internal QString value.
      * @note This isn't a member of UIConverter since it's used for
      *       legacy extra-data settings saving routine only. */
    static QString updateChannelToInternalString(KUpdateChannel enmUpdateChannel);
    /** Converts passed @a strUpdateChannel to KUpdateChannel value.
      * @note This isn't a member of UIConverter since it's used for
      *       legacy extra-data settings saving routine only. */
    static KUpdateChannel updateChannelFromInternalString(const QString &strUpdateChannel);

private:

    /** Decodes data. */
    void decode();
    /** Encodes data. */
    void encode();

    /** Holds the populated list of update options. */
    static VBoxUpdateDayList m_dayList;

    /** Holds the update data. */
    QString         m_strData;
    /** Holds the update period index. */
    PeriodType      m_enmPeriodIndex;
    /** Holds the update date. */
    QDate           m_date;
    /** Holds the update channel. */
    KUpdateChannel  m_enmUpdateChannel;
    /** Holds the update version. */
    UIVersion       m_version;
};


#endif /* !FEQT_INCLUDED_SRC_networking_UIUpdateDefs_h */

