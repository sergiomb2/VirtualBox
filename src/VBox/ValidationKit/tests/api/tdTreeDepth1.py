#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Medium and Snapshot Tree Depth Test #1
"""

__copyright__ = \
"""
Copyright (C) 2010-2022 Oracle Corporation

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


# Standard Python imports.
import os
import sys
import random

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0]
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(g_ksValidationKitDir)

# Validation Kit imports.
from testdriver import base
from testdriver import reporter
from testdriver import vboxcon


class SubTstDrvTreeDepth1(base.SubTestDriverBase):
    """
    Sub-test driver for Medium and Snapshot Tree Depth Test #1.
    """

    def __init__(self, oTstDrv):
        base.SubTestDriverBase.__init__(self, oTstDrv, 'tree-depth', 'Media and Snapshot tree depths');

    def testIt(self):
        """
        Execute the sub-testcase.
        """
        return  self.testMediumTreeDepth() \
            and self.testSnapshotTreeDepth()

    #
    # Test execution helpers.
    #

    def testMediumTreeDepth(self):
        """
        Test medium tree depth.
        """
        reporter.testStart('mediumTreeDepth')

        try:
            oVBox = self.oTstDrv.oVBoxMgr.getVirtualBox()
            oVM = self.oTstDrv.createTestVM('test-medium', 1, None, 4)
            assert oVM is not None

            # create chain with 300 disk images (medium tree depth limit)
            fRc = True
            oSession = self.oTstDrv.openSession(oVM)
            cImages = 38 #00
            for i in range(1, cImages + 1):
                sHddPath = os.path.join(self.oTstDrv.sScratchPath, 'Test' + str(i) + '.vdi')
                if i == 1:
                    oHd = oSession.createBaseHd(sHddPath, cb=1024*1024)
                else:
                    oHd = oSession.createDiffHd(oHd, sHddPath)
                if oHd is None:
                    fRc = False
                    break

            # modify the VM config, attach HDD
            fRc = fRc and oSession.attachHd(sHddPath, sController='SATA Controller', fImmutable=False, fForceResource=False)
            fRc = fRc and oSession.saveSettings()
            fRc = oSession.close() and fRc
            ## @todo r=klaus: count known hard disk images, should be cImages

            # unregister, making sure the images are closed
            sSettingsFile = oVM.settingsFilePath
            fDetachAll = random.choice([False, True])
            if fDetachAll:
                reporter.log('unregistering VM, DetachAll style')
            else:
                reporter.log('unregistering VM, UnregisterOnly style')
            self.oTstDrv.forgetTestMachine(oVM)
            if fDetachAll:
                aoHDs = oVM.unregister(vboxcon.CleanupMode_DetachAllReturnHardDisksOnly)
                for oHD in aoHDs:
                    oHD.close()
                aoHDs = None
            else:
                oVM.unregister(vboxcon.CleanupMode_UnregisterOnly)
            oVM = None
            
            # If there is no base image (expected) then there are no leftover
            # child images either. Can be changed later once the todos above
            # and below are resolved.
            cBaseImages = len(self.oTstDrv.oVBoxMgr.getArray(oVBox, 'hardDisks'))
            reporter.log('API reports %i base images' % (cBaseImages))
            fRc = fRc and cBaseImages == 0

            # re-register to test loading of settings
            reporter.log('opening VM %s, testing config reading' % (sSettingsFile))
            oVM = oVBox.openMachine(sSettingsFile)
            ## @todo r=klaus: count known hard disk images, should be cImages

            reporter.log('unregistering VM')
            oVM.unregister(vboxcon.CleanupMode_UnregisterOnly)
            oVM = None

            cBaseImages = len(self.oTstDrv.oVBoxMgr.getArray(oVBox, 'hardDisks'))
            reporter.log('API reports %i base images' % (cBaseImages))
            fRc = fRc and cBaseImages == 0

            assert fRc is True
        except:
            reporter.errorXcpt()

        return reporter.testDone()[1] == 0

    def testSnapshotTreeDepth(self):
        """
        Test snapshot tree depth.
        """
        reporter.testStart('snapshotTreeDepth')

        try:
            oVBox = self.oTstDrv.oVBoxMgr.getVirtualBox()
            oVM = self.oTstDrv.createTestVM('test-snap', 1, None, 4)
            assert oVM is not None

            # modify the VM config, create and attach empty HDD
            oSession = self.oTstDrv.openSession(oVM)
            sHddPath = os.path.join(self.oTstDrv.sScratchPath, 'TestSnapEmpty.vdi')
            fRc = True
            fRc = fRc and oSession.createAndAttachHd(sHddPath, cb=1024*1024, sController='SATA Controller', fImmutable=False)
            fRc = fRc and oSession.saveSettings()

            # take 250 snapshots (snapshot tree depth limit)
            cSnapshots = 13 #00
            for i in range(1, cSnapshots + 1):
                fRc = fRc and oSession.takeSnapshot('Snapshot ' + str(i))
            fRc = oSession.close() and fRc
            oSession = None
            reporter.log('API reports %i snapshots' % (oVM.snapshotCount))
            fRc = fRc and oVM.snapshotCount == cSnapshots

            assert fRc is True

            # unregister, making sure the images are closed
            sSettingsFile = oVM.settingsFilePath
            fDetachAll = random.choice([False, True])
            if fDetachAll:
                reporter.log('unregistering VM, DetachAll style')
            else:
                reporter.log('unregistering VM, UnregisterOnly style')
            self.oTstDrv.forgetTestMachine(oVM)
            if fDetachAll:
                aoHDs = oVM.unregister(vboxcon.CleanupMode_DetachAllReturnHardDisksOnly)
                for oHD in aoHDs:
                    oHD.close()
                aoHDs = None
            else:
                oVM.unregister(vboxcon.CleanupMode_UnregisterOnly)
            oVM = None

            # If there is no base image (expected) then there are no leftover
            # child images either. Can be changed later once the todos above
            # and below are resolved.
            cBaseImages = len(self.oTstDrv.oVBoxMgr.getArray(oVBox, 'hardDisks'))
            reporter.log('API reports %i base images' % (cBaseImages))
            fRc = fRc and cBaseImages == 0

            # re-register to test loading of settings
            reporter.log('opening VM %s, testing config reading' % (sSettingsFile))
            oVM = oVBox.openMachine(sSettingsFile)
            reporter.log('API reports %i snapshots' % (oVM.snapshotCount))
            fRc = fRc and oVM.snapshotCount == cSnapshots

            reporter.log('unregistering VM')
            oVM.unregister(vboxcon.CleanupMode_UnregisterOnly)
            oVM = None

            cBaseImages = len(self.oTstDrv.oVBoxMgr.getArray(oVBox, 'hardDisks'))
            reporter.log('API reports %i base images' % (cBaseImages))
            fRc = fRc and cBaseImages == 0

            assert fRc is True
        except:
            reporter.errorXcpt()

        return reporter.testDone()[1] == 0


if __name__ == '__main__':
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    from tdApi1 import tdApi1;      # pylint: disable=relative-import
    sys.exit(tdApi1([SubTstDrvTreeDepth1]).main(sys.argv))

