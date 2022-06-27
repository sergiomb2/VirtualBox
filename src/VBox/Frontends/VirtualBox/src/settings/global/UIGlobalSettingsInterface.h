/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsInterface class declaration.
 */

/*
 * Copyright (C) 2008-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsInterface_h
#define FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsInterface_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* Forward declarations: */
class UIColorThemeEditor;
struct UIDataSettingsGlobalInterface;
typedef UISettingsCache<UIDataSettingsGlobalInterface> UISettingsCacheGlobalInterface;

/** Global settings: User Interface page. */
class SHARED_LIBRARY_STUFF UIGlobalSettingsInterface : public UISettingsPageGlobal
{
    Q_OBJECT;

public:

    /** Constructs User Interface settings page. */
    UIGlobalSettingsInterface();
    /** Destructs User Interface settings page. */
    virtual ~UIGlobalSettingsInterface() RT_OVERRIDE;

protected:

    /** Loads settings from external object(s) packed inside @a data to cache.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void loadToCacheFrom(QVariant &data) RT_OVERRIDE;
    /** Loads data from cache to corresponding widgets.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void getFromCache() RT_OVERRIDE;

    /** Saves data from corresponding widgets to cache.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void putToCache() RT_OVERRIDE;
    /** Saves settings from cache to external object(s) packed inside @a data.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void saveFromCacheTo(QVariant &data) RT_OVERRIDE;

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Cleanups all. */
    void cleanup();

    /** Saves existing data from cache. */
    bool saveData();

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalInterface *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the color theme label instance. */
        UIColorThemeEditor *m_pEditorColorTheme;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsInterface_h */
