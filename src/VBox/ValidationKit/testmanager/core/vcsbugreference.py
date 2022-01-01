# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager - VcsBugReferences
"""

__copyright__ = \
"""
Copyright (C) 2012-2022 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL) only, as it comes in the "COPYING.CDDL" file of the
VirtualBox OSE distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.
"""
__version__ = "$Revision$"


# Standard python imports.
import unittest;

# Validation Kit imports.
from testmanager.core.base              import ModelDataBase, ModelDataBaseTestCase, ModelLogicBase, TMExceptionBase;


class VcsBugReferenceData(ModelDataBase):
    """
    A version control system (VCS) bug tracker reference (commit message tag).
    """

    #kasIdAttr = ['sRepository','iRevision', 'sBugTracker', 'iBugNo'];

    ksParam_sRepository         = 'VcsBugReference_sRepository';
    ksParam_iRevision           = 'VcsBugReference_iRevision';
    ksParam_sBugTracker         = 'VcsBugReference_sBugTracker';
    ksParam_lBugNo              = 'VcsBugReference_lBugNo';

    kasAllowNullAttributes      = [ ];

    def __init__(self):
        ModelDataBase.__init__(self);

        #
        # Initialize with defaults.
        # See the database for explanations of each of these fields.
        #
        self.sRepository        = None;
        self.iRevision          = None;
        self.sBugTracker        = None;
        self.lBugNo             = None;

    def initFromDbRow(self, aoRow):
        """
        Re-initializes the object from a SELECT * FROM VcsBugReferences row.
        Returns self.  Raises exception if aoRow is None.
        """
        if aoRow is None:
            raise TMExceptionBase('VcsBugReference not found.');

        self.sRepository        = aoRow[0];
        self.iRevision          = aoRow[1];
        self.sBugTracker        = aoRow[2];
        self.lBugNo             = aoRow[3];
        return self;

    def initFromValues(self, sRepository, iRevision, sBugTracker, lBugNo):
        """
        Reinitializes form a set of values.
        return self.
        """
        self.sRepository        = sRepository;
        self.iRevision          = iRevision;
        self.sBugTracker        = sBugTracker;
        self.lBugNo             = lBugNo;
        return self;


class VcsBugReferenceDataEx(VcsBugReferenceData):
    """
    Extended version of VcsBugReferenceData that includes the commit details.
    """
    def __init__(self):
        VcsBugReferenceData.__init__(self);
        self.tsCreated          = None;
        self.sAuthor            = None;
        self.sMessage           = None;

    def initFromDbRow(self, aoRow):
        VcsBugReferenceData.initFromDbRow(self, aoRow);
        self.tsCreated          = aoRow[4];
        self.sAuthor            = aoRow[5];
        self.sMessage           = aoRow[6];
        return self;


class VcsBugReferenceLogic(ModelLogicBase): # pylint: disable=too-few-public-methods
    """
    VCS revision <-> bug tracker references database logic.
    """

    #
    # Standard methods.
    #

    def fetchForListing(self, iStart, cMaxRows, tsNow, aiSortColumns = None):
        """
        Fetches VCS revisions for listing.

        Returns an array (list) of VcsBugReferenceData items, empty list if none.
        Raises exception on error.
        """
        _ = tsNow; _ = aiSortColumns;
        self._oDb.execute('''
SELECT  *
FROM    VcsBugReferences
ORDER BY sRepository, iRevision, sBugTracker, lBugNo
LIMIT %s OFFSET %s
''', (cMaxRows, iStart,));

        aoRows = [];
        for _ in range(self._oDb.getRowCount()):
            aoRows.append(VcsBugReferenceData().initFromDbRow(self._oDb.fetchOne()));
        return aoRows;

    def exists(self, oData):
        """
        Checks if the data is already present in the DB.
        Returns True / False.
        Raises exception on input and database errors.
        """
        self._oDb.execute('''
SELECT  COUNT(*)
FROM    VcsBugReferences
WHERE   sRepository = %s
    AND iRevision   = %s
    AND sBugTracker = %s
    AND lBugNo      = %s
''', ( oData.sRepository, oData.iRevision, oData.sBugTracker, oData.lBugNo));
        cRows = self._oDb.fetchOne()[0];
        if cRows < 0 or cRows > 1:
            raise TMExceptionBase('VcsBugReferences has a primary key problem: %u duplicates' % (cRows,));
        return cRows != 0;


    #
    # Other methods.
    #

    def addVcsBugReference(self, oData, fCommit = False):
        """
        Adds (or updates) a tree revision record.
        Raises exception on input and database errors.
        """

        # Check VcsBugReferenceData before do anything
        dDataErrors = oData.validateAndConvert(self._oDb, oData.ksValidateFor_Add);
        if dDataErrors:
            raise TMExceptionBase('Invalid data passed to addVcsBugReference(): %s' % (dDataErrors,));

        # Does it already exist?
        if not self.exists(oData):
            # New row.
            self._oDb.execute('INSERT INTO VcsBugReferences (sRepository, iRevision, sBugTracker, lBugNo)\n'
                              'VALUES (%s, %s, %s, %s)\n'
                              , ( oData.sRepository,
                                  oData.iRevision,
                                  oData.sBugTracker,
                                  oData.lBugNo,
                              ));

        self._oDb.maybeCommit(fCommit);
        return oData;

    def getLastRevision(self, sRepository):
        """
        Get the last known revision number for the given repository, returns 0
        if the repository is not known to us:
        """
        self._oDb.execute('''
SELECT iRevision
FROM   VcsBugReferences
WHERE  sRepository = %s
ORDER BY iRevision DESC
LIMIT 1
''', ( sRepository, ));
        if self._oDb.getRowCount() == 0:
            return 0;
        return self._oDb.fetchOne()[0];

    def fetchForBug(self, sBugTracker, lBugNo):
        """
        Fetches VCS revisions for a bug.

        Returns an array (list) of VcsBugReferenceDataEx items, empty list if none.
        Raises exception on error.
        """
        self._oDb.execute('''
SELECT  VcsBugReferences.*,
        VcsRevisions.tsCreated,
        VcsRevisions.sAuthor,
        VcsRevisions.sMessage
FROM    VcsBugReferences
LEFT OUTER JOIN VcsRevisions ON (    VcsRevisions.sRepository = VcsBugReferences.sRepository
                                 AND VcsRevisions.iRevision   = VcsBugReferences.iRevision )
WHERE   sBugTracker = %s
    AND lBugNo      = %s
ORDER BY VcsRevisions.tsCreated, VcsBugReferences.sRepository, VcsBugReferences.iRevision
''', (sBugTracker, lBugNo,));

        aoRows = [];
        for _ in range(self._oDb.getRowCount()):
            aoRows.append(VcsBugReferenceDataEx().initFromDbRow(self._oDb.fetchOne()));
        return aoRows;


#
# Unit testing.
#

# pylint: disable=missing-docstring
class VcsBugReferenceDataTestCase(ModelDataBaseTestCase):
    def setUp(self):
        self.aoSamples = [VcsBugReferenceData(),];

if __name__ == '__main__':
    unittest.main();
    # not reached.

