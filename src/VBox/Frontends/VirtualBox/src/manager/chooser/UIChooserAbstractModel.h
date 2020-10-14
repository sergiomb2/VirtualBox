/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserAbstractModel class declaration.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserAbstractModel_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserAbstractModel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIChooserDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declaration: */
class QUuid;
class UIChooser;
class UIChooserNode;
class UITask;
class CCloudMachine;
class CMachine;

/** QObject extension used as VM Chooser-pane abstract model.
  * This class is used to load/save a tree of abstract invisible
  * nodes representing VMs and their groups from/to extra-data. */
class UIChooserAbstractModel : public QObject
{
    Q_OBJECT;

signals:

    /** @name Cloud machine stuff.
      * @{ */
        /** Notifies listeners about state change for cloud machine with certain @a uId. */
        void sigCloudMachineStateChange(const QUuid &uId);
    /** @} */

    /** @name Group saving stuff.
      * @{ */
        /** Commands to start group saving. */
        void sigStartGroupSaving();
        /** Notifies listeners about group saving state changed. */
        void sigGroupSavingStateChanged();
    /** @} */

public:

    /** Constructs abstract Chooser-model passing @a pParent to the base-class. */
    UIChooserAbstractModel(UIChooser *pParent);

    /** @name General stuff.
      * @{ */
        /** Inits model. */
        virtual void init();
        /** Deinits model. */
        virtual void deinit();
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Returns invisible root node instance. */
        UIChooserNode *invisibleRoot() const { return m_pInvisibleRootNode; }

        /** Wipes out empty groups. */
        void wipeOutEmptyGroups();

        /** Returns possible group node names for machine node with passed @a uId to move to. */
        QStringList possibleGroupNodeNamesForMachineNodeToMove(const QUuid &uId);
        /** Returns possible group node names for group node with passed @a strFullName to move to. */
        QStringList possibleGroupNodeNamesForGroupNodeToMove(const QString &strFullName);

        /** Generates unique group name traversing recursively starting from @a pRoot. */
        static QString uniqueGroupName(UIChooserNode *pRoot);
    /** @} */

    /** @name Search stuff.
      * @{ */
        /** Performs a search using @a strSearchTerm and @a iSearchFlags specified. */
        virtual void performSearch(const QString &strSearchTerm, int iSearchFlags);
        /** Resets the search result data members and disables item's visual effects.
          * Also returns a list of all nodes which may be utilized by the calling code. */
        virtual QList<UIChooserNode*> resetSearch();
        /** Returns search result. */
        QList<UIChooserNode*> searchResult() const;
    /** @} */

    /** @name Group saving stuff.
      * @{ */
        /** Commands to save groups. */
        void saveGroups();
        /** Returns whether group saving is in progress. */
        bool isGroupSavingInProgress() const;

        /** Returns QString representation for passed @a uId, wiping out {} symbols.
          * @note  Required for backward compatibility after QString=>QUuid change. */
        static QString toOldStyleUuid(const QUuid &uId);

        /** Returns node extra-data prefix of certain @a enmType. */
        static QString prefixToString(UIChooserNodeDataPrefixType enmType);
        /** Returns node extra-data option of certain @a enmType. */
        static QString optionToString(UIChooserNodeDataOptionType enmType);
        /** Returns node extra-data value of certain @a enmType. */
        static QString valueToString(UIChooserNodeDataValueType enmType);
    /** @} */

public slots:

    /** @name Cloud machine stuff.
      * @{ */
        /** Handles cloud machine state change. */
        void sltHandleCloudMachineStateChange();
    /** @} */

    /** @name Group saving stuff.
      * @{ */
        /** Handles group settings saving complete. */
        void sltGroupSettingsSaveComplete();
        /** Handles group definitions saving complete. */
        void sltGroupDefinitionsSaveComplete();
    /** @} */

protected slots:

    /** @name Main event handling stuff.
      * @{ */
        /** Handles local machine @a enmState change for machine with certain @a uMachineId. */
        virtual void sltLocalMachineStateChanged(const QUuid &uMachineId, const KMachineState enmState);
        /** Handles local machine data change for machine with certain @a uMachineId. */
        virtual void sltLocalMachineDataChanged(const QUuid &uMachineId);
        /** Handles local machine registering/unregistering for machine with certain @a uMachineId. */
        virtual void sltLocalMachineRegistrationChanged(const QUuid &uMachineId, const bool fRegistered);
        /** Handles cloud machine registering/unregistering for machine with certain @a uMachineId.
          * @param  strProviderShortName  Brings provider short name.
          * @param  strProfileName        Brings profile name. */
        virtual void sltCloudMachineRegistrationChanged(const QString &strProviderShortName, const QString &strProfileName,
                                                        const QUuid &uMachineId, const bool fRegistered);
        /** Handles session @a enmState change for machine with certain @a uMachineId. */
        virtual void sltSessionStateChanged(const QUuid &uMachineId, const KSessionState enmState);
        /** Handles snapshot change for machine/snapshot with certain @a uMachineId / @a uSnapshotId. */
        virtual void sltSnapshotChanged(const QUuid &uMachineId, const QUuid &uSnapshotId);
        /** Handles event about cloud provider with @a uProviderId being uninstalled. */
        virtual void sltHandleCloudProviderUninstall(const QUuid &uProviderId);
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Handles reload machine with certain @a uMachineId request. */
        virtual void sltReloadMachine(const QUuid &uMachineId);
    /** @} */

    /** @name Cloud stuff.
      * @{ */
        /** Handles list cloud machines task complete signal. */
        virtual void sltHandleCloudListMachinesTaskComplete(UITask *pTask);
        /** Handles Cloud Profile Manager cumulative change. */
        virtual void sltHandleCloudProfileManagerCumulativeChange();
    /** @} */

private slots:

    /** @name Group saving stuff.
      * @{ */
        /** Handles request to start group saving. */
        void sltStartGroupSaving();
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares connections. */
        void prepareConnections();
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Reloads local tree. */
        void reloadLocalTree();
        /** Reloads cloud tree. */
        void reloadCloudTree();

        /** Adds local machine item based on certain @a comMachine and optionally @a fMakeItVisible. */
        void addLocalMachineIntoTheTree(const CMachine &comMachine, bool fMakeItVisible = false);
        /** Adds cloud machine item based on certain @a comMachine and optionally @a fMakeItVisible, into @a strGroup. */
        void addCloudMachineIntoTheTree(const QString &strGroup, const CCloudMachine &comMachine, bool fMakeItVisible = false);

        /** Acquires local group node, creates one if necessary.
          * @param  strName           Brings the name of group we looking for.
          * @param  pParentNode       Brings the parent we starting to look for a group from.
          * @param  fAllGroupsOpened  Brings whether we should open all the groups till the required one. */
        UIChooserNode *getLocalGroupNode(const QString &strName, UIChooserNode *pParentNode, bool fAllGroupsOpened);
        /** Acquires cloud group node, never create new, returns root if nothing found.
          * @param  strName           Brings the name of group we looking for.
          * @param  pParentNode       Brings the parent we starting to look for a group from.
          * @param  fAllGroupsOpened  Brings whether we should open all the groups till the required one. */
        UIChooserNode *getCloudGroupNode(const QString &strName, UIChooserNode *pParentNode, bool fAllGroupsOpened);

        /** Returns whether group node * with specified @a enmDataType and @a strName should be opened,
          * searching starting from the passed @a pParentNode. */
        bool shouldGroupNodeBeOpened(UIChooserNode *pParentNode,
                                     UIChooserNodeDataPrefixType enmDataType,
                                     const QString &strName) const;
        /** Returns whether global node should be favorite,
          * searching starting from the passed @a pParentNode. */
        bool shouldGlobalNodeBeFavorite(UIChooserNode *pParentNode) const;

        /** Wipes out empty groups starting from @a pParentItem. */
        void wipeOutEmptyGroupsStartingFrom(UIChooserNode *pParentNode);

        /** Acquires desired position for a child of @a pParentNode with specified @a enmDataType and @a strName. */
        int getDesiredNodePosition(UIChooserNode *pParentNode, UIChooserNodeDataPrefixType enmDataType, const QString &strName);
        /** Acquires defined position for a child of @a pParentNode with specified @a enmDataType and @a strName. */
        int getDefinedNodePosition(UIChooserNode *pParentNode, UIChooserNodeDataPrefixType enmDataType, const QString &strName);

        /** Creates local machine node based on certain @a comMachine as a child of specified @a pParentNode. */
        void createLocalMachineNode(UIChooserNode *pParentNode, const CMachine &comMachine);
        /** Creates cloud machine node based on certain @a comMachine as a child of specified @a pParentNode. */
        void createCloudMachineNode(UIChooserNode *pParentNode, const CCloudMachine &comMachine);

        /** Gathers a list of possible group node names for machine nodes listed in @a exceptions, starting from @a pCurrentNode. */
        QStringList gatherPossibleGroupNodeNames(UIChooserNode *pCurrentNode, QList<UIChooserNode*> exceptions) const;
    /** @} */

    /** @name Group saving stuff.
      * @{ */
        /** Saves group settings. */
        void saveGroupSettings();
        /** Saves group definitions. */
        void saveGroupDefinitions();

        /** Gathers group @a settings of @a pParentGroup. */
        void gatherGroupSettings(QMap<QString, QStringList> &settings, UIChooserNode *pParentGroup);
        /** Gathers group @a definitions of @a pParentGroup. */
        void gatherGroupDefinitions(QMap<QString, QStringList> &definitions, UIChooserNode *pParentGroup);

        /** Makes sure group settings saving is finished. */
        void makeSureGroupSettingsSaveIsFinished();
        /** Makes sure group definitions saving is finished. */
        void makeSureGroupDefinitionsSaveIsFinished();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the parent widget reference. */
        UIChooser *m_pParent;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Holds the invisible root node instance. */
        UIChooserNode *m_pInvisibleRootNode;
    /** @} */

    /** @name Search stuff.
      * @{ */
        /** Stores the results of the current search. */
        QList<UIChooserNode*>  m_searchResults;
    /** @} */

    /** @name Group saving stuff.
      * @{ */
        /** Holds the consolidated map of group settings/definitions. */
        QMap<QString, QStringList>  m_groups;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserAbstractModel_h */
