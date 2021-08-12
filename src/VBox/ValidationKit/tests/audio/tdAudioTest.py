# -*- coding: utf-8 -*-
# $Id$

"""
AudioTest test driver which invokes the VKAT (Validation Kit Audio Test)
binary to perform the actual audio tests.

The generated test set archive on the guest will be downloaded by TXS
to the host for later audio comparison / verification.
"""

__copyright__ = \
"""
Copyright (C) 2021 Oracle Corporation

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
import signal
import subprocess
import uuid

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from testdriver import reporter
from testdriver import base
from testdriver import vbox
from testdriver import vboxtestvms
from common     import utils;

# pylint: disable=unnecessary-semicolon

class tdAudioTest(vbox.TestDriver):
    """
    Runs various audio tests.
    """
    def __init__(self):
        vbox.TestDriver.__init__(self);
        self.oTestVmSet       = self.oTestVmManager.getSmokeVmSet('nat');
        self.asGstVkatPaths   = [
            # Debugging stuff (SCP'd over to the guest).
            '/tmp/vkat',
            '/tmp/VBoxAudioTest',
            # Validation Kit .ISO.
            '${CDROM}/vboxvalidationkit/${OS/ARCH}/vkat${EXESUFF}',
            '${CDROM}/${OS/ARCH}/vkat${EXESUFF}',
            ## @odo VBoxAudioTest on Guest Additions?
        ];
        self.asTestsDef       = [
            'guest_tone_playback', 'guest_tone_recording'
        ];
        self.asTests          = self.asTestsDef;

        # Enable audio debug mode.
        #
        # This is needed in order to load and use the Validation Kit audio driver,
        # which in turn is being used in conjunction with the guest side to record
        # output (guest is playing back) and injecting input (guest is recording).
        self.asOptExtraData   = [
            'VBoxInternal2/Audio/Debug/Enabled:true',
        ];

        # Name of the running VM to use for running the test driver. Optional, and None if not being used.
        self.sRunningVmName   = None;

    def showUsage(self):
        """
        Shows the audio test driver-specific command line options.
        """
        fRc = vbox.TestDriver.showUsage(self);
        reporter.log('');
        reporter.log('tdAudioTest Options:');
        reporter.log('  --runningvmname <vmname>');
        reporter.log('  --audio-tests   <s1[:s2[:]]>');
        reporter.log('      Default: %s  (all)' % (':'.join(self.asTestsDef)));
        return fRc;

    def parseOption(self, asArgs, iArg):
        """
        Parses the audio test driver-specific command line options.
        """
        if asArgs[iArg] == '--runningvmname':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--runningvmname" needs VM name');

            self.sRunningVmName = asArgs[iArg];
        elif asArgs[iArg] == '--audio-tests':
            iArg += 1;
            if asArgs[iArg] == 'all': # Nice for debugging scripts.
                self.asTests = self.asTestsDef;
            else:
                self.asTests = asArgs[iArg].split(':');
                for s in self.asTests:
                    if s not in self.asTestsDef:
                        raise base.InvalidOption('The "--audio-tests" value "%s" is not valid; valid values are: %s'
                                                    % (s, ' '.join(self.asTestsDef)));
        else:
            return vbox.TestDriver.parseOption(self, asArgs, iArg);
        return iArg + 1;

    def actionVerify(self):
        """
        Verifies the test driver before running.
        """
        if self.sVBoxValidationKitIso is None or not os.path.isfile(self.sVBoxValidationKitIso):
            reporter.error('Cannot find the VBoxValidationKit.iso! (%s)'
                           'Please unzip a Validation Kit build in the current directory or in some parent one.'
                           % (self.sVBoxValidationKitIso,) );
            return False;
        return vbox.TestDriver.actionVerify(self);

    def actionConfig(self):
        """
        Configures the test driver before running.
        """
        if not self.importVBoxApi(): # So we can use the constant below.
            return False;

        # Make sure that the Validation Kit .ISO is mounted
        # to find the VKAT (Validation Kit Audio Test) binary on it.
        assert self.sVBoxValidationKitIso is not None;
        return self.oTestVmSet.actionConfig(self, sDvdImage = self.sVBoxValidationKitIso);

    def actionExecute(self):
        """
        Executes the test driver.
        """
        if self.sRunningVmName is None:
            return self.oTestVmSet.actionExecute(self, self.testOneVmConfig);
        return self.actionExecuteOnRunnigVM();

    def actionExecuteOnRunnigVM(self):
        """
        Executes the tests in an already configured + running VM.
        """
        if not self.importVBoxApi():
            return False;

        fRc = True;

        oVirtualBox = self.oVBoxMgr.getVirtualBox();
        try:
            oVM = oVirtualBox.findMachine(self.sRunningVmName);
            if oVM.state != self.oVBoxMgr.constants.MachineState_Running:
                reporter.error("Machine '%s' is not in Running state" % (self.sRunningVmName));
                fRc = False;
        except:
            reporter.errorXcpt("Machine '%s' not found" % (self.sRunningVmName));
            fRc = False;

        if fRc:
            oSession = self.openSession(oVM);
            if oSession:
                # Tweak this to your likings.
                oTestVm = vboxtestvms.TestVm('runningvm', sKind = 'Ubuntu_64');
                (fRc, oTxsSession) = self.txsDoConnectViaTcp(oSession, 30 * 1000);
                if fRc:
                    self.doTest(oTestVm, oSession, oTxsSession);
            else:
                reporter.error("Unable to open session for machine '%s'" % (self.sRunningVmName));
                fRc = False;

        del oVM;
        del oVirtualBox;
        return fRc;

    def getGstVkatLogFilePath(self, oTestVm):
        """
        Returns the log file path of VKAT running on the guest (daemonized).
        """
        return oTestVm.pathJoin(self.getGuestTempDir(oTestVm), 'vkat-guest-daemonized.log');

    def locateGstVkat(self, oSession, oTxsSession):
        """
        Returns guest side path to VKAT.
        """
        for sVkatPath in self.asGstVkatPaths:
            reporter.log2('Checking for VKAT at: %s ...' % (sVkatPath));
            if self.txsIsFile(oSession, oTxsSession, sVkatPath):
                return (True, sVkatPath);
        reporter.error('Unable to find guest VKAT in any of these places:\n%s' % ('\n'.join(self.asGstVkatPaths),));
        return (False, "");

    def killHstProcessByName(self, sProcName):
        """
        Kills processes by their name.
        """
        reporter.log('Trying to kill processes named "%s"' % (sProcName,));
        if sys.platform == 'win32':
            os.system('taskkill /IM "%s.exe" /F' % (sProcName));
        else: # Note: killall is not available on older Debians (requires psmisc).
            # Using the BSD syntax here; MacOS also should understand this.
            procPs = subprocess.Popen(['ps', 'ax'], stdout=subprocess.PIPE);
            out, _ = procPs.communicate();
            for sLine in out.decode("utf-8").splitlines():
                if sProcName in sLine:
                    pid = int(sLine.split(None, 1)[0]);
                    reporter.log2('Killing PID %d' % (pid,));
                    os.kill(pid, signal.SIGKILL); # pylint: disable=no-member

    def killHstVkat(self):
        """
        Kills VKAT (VBoxAudioTest) on the host side.
        """
        reporter.log('Killing stale/old VKAT processes ...');
        self.killHstProcessByName("vkat");
        self.killHstProcessByName("VBoxAudioTest");

    def getWinFirewallArgsDisable(self, sOsType):
        """
        Returns the command line arguments for Windows OSes
        to disable the built-in firewall (if any).

        If not supported, returns an empty array.
        """
        if   sOsType == 'vista': # pylint: disable=no-else-return
             # Vista and up.
            return (['netsh.exe', 'advfirewall', 'set', 'allprofiles', 'state', 'off']);
        elif sOsType == 'xp':   # Older stuff (XP / 2003).
            return(['netsh.exe', 'firewall', 'set', 'opmode', 'mode=DISABLE']);
        # Not supported / available.
        return [];

    def disableGstFirewall(self, oTestVm, oTxsSession):
        """
        Disables the firewall on a guest (if any).

        Needs elevated / admin / root privileges.

        Returns success status, not logged.
        """
        fRc = False;

        asArgs  = [];
        sOsType = '';
        if oTestVm.isWindows():
            if oTestVm.sKind in ['WindowsNT4', 'WindowsNT3x']:
                sOsType = 'nt3x'; # Not supported, but define it anyway.
            elif oTestVm.sKind in ('Windows2000', 'WindowsXP', 'Windows2003'):
                sOsType = 'xp';
            else:
                sOsType = 'vista';
            asArgs = self.getWinFirewallArgsDisable(sOsType);
        else:
            sOsType = 'unsupported';

        reporter.log('Disabling firewall on guest (type: %s) ...' % (sOsType,));

        if asArgs:
            fRc = self.txsRunTest(oTxsSession, 'Disabling guest firewall', 3 * 60 * 1000, \
                                  oTestVm.pathJoin(self.getGuestSystemDir(oTestVm), asArgs[0]), asArgs);
            if not fRc:
                reporter.error('Disabling firewall on guest returned exit code error %d' % (self.getLastRcFromTxs(oTxsSession)));
        else:
            reporter.log('Firewall not available on guest, skipping');
            fRc = True; # Not available, just skip.

        return fRc;

    def disableHstFirewall(self):
        """
        Disables the firewall on the host (if any).

        Needs elevated / admin / root privileges.

        Returns success status, not logged.
        """
        fRc = False;

        asArgs  = [];
        sOsType = sys.platform;

        if sOsType == 'win32':
            reporter.log('Disabling firewall on host (type: %s) ...' % (sOsType));

            ## @todo For now we ASSUME that we don't run (and don't support even) on old(er)
            #        Windows hosts than Vista.
            asArgs = self.getWinFirewallArgsDisable('vista');

        if asArgs:
            oProcess = utils.sudoProcessPopen(asArgs, stdout=subprocess.PIPE, stdin=subprocess.PIPE,
                                              stderr=subprocess.PIPE, shell = False, close_fds = False);
            if oProcess:
                sOut, sErr = oProcess.communicate();

                sOut = sOut.decode(sys.stdin.encoding);
                for sLine in sOut.split('\n'):
                    reporter.log(sLine);

                sErr = sErr.decode(sys.stdin.encoding);
                for sLine in sErr.split('\n'):
                    reporter.log(sLine);

                iExitCode = oProcess.poll();
                if iExitCode == 0:
                    fRc = True;
                else:
                    reporter.error('Disabling firewall on host returned exit code error %d' % iExitCode);
            else:
                fRc = False;

            if not fRc:
                reporter.error('Disabling firewall on host failed');
        else:
            reporter.log('Firewall not available on host, skipping');
            fRc = True; # Not available, just skip.

        return fRc;

    def getLastRcFromTxs(self, oTxsSession):
        """
        Extracts the last exit code reported by TXS from a run before.
        Assumes that nothing else has been run on the same TXS session in the meantime.
        """
        iRc = 0;
        (_, sOpcode, abPayload) = oTxsSession.getLastReply();
        if sOpcode.startswith('PROC NOK '): # Extract process rc
            iRc = abPayload[0]; # ASSUMES 8-bit rc for now.
        return iRc;

    def startVkatOnGuest(self, oTestVm, oSession, oTxsSession):
        """
        Starts VKAT on the guest (running in background).
        """
        sPathTemp      = self.getGuestTempDir(oTestVm);
        sPathAudioOut  = oTestVm.pathJoin(sPathTemp, 'vkat-guest-out');
        sPathAudioTemp = oTestVm.pathJoin(sPathTemp, 'vkat-guest-temp');

        reporter.log('Guest audio test temp path is \"%s\"' % (sPathAudioOut));
        reporter.log('Guest audio test output path is \"%s\"' % (sPathAudioTemp));

        fRc, sVkatExe = self.locateGstVkat(oSession, oTxsSession);
        if fRc:
            reporter.log('Using VKAT on guest at \"%s\"' % (sVkatExe));

            aArgs     = [ sVkatExe, 'test', '-vvvv', '--mode', 'guest', \
                                    '--tempdir', sPathAudioTemp, '--outdir', sPathAudioOut ];
            #
            # Start VKAT in the background (daemonized) on the guest side, so that we
            # can continue starting VKAT on the host.
            #
            aArgs.extend(['--daemonize']);

            #
            # Add own environment stuff.
            #
            aEnv = [];

            # Write the log file to some deterministic place so TxS can retrieve it later.
            sVkatLogFile = 'VKAT_RELEASE_LOG_DEST=file=' + self.getGstVkatLogFilePath(oTestVm);
            aEnv.extend([ sVkatLogFile ]);

            #
            # Execute.
            #
            fRc = self.txsRunTest(oTxsSession, 'Starting VKAT on guest', 15 * 60 * 1000,
                                  sVkatExe, aArgs, aEnv);
            if not fRc:
                reporter.error('VKAT on guest returned exit code error %d' % (self.getLastRcFromTxs(oTxsSession)));
        else:
            reporter.error('VKAT on guest not found');

        return fRc;

    def runTests(self, oTestVm, oSession, oTxsSession, sDesc, sTests):
        """
        Runs one or more tests using VKAT on the host, which in turn will
        communicate with VKAT running on the guest and the Validation Kit
        audio driver ATS (Audio Testing Service).
        """
        _              = oSession, oTxsSession;
        sTag           = uuid.uuid4();

        sPathTemp      = self.sScratchPath;
        sPathAudioOut  = oTestVm.pathJoin(sPathTemp, 'vkat-host-out-%s' % (sTag));
        sPathAudioTemp = oTestVm.pathJoin(sPathTemp, 'vkat-host-temp-%s' % (sTag));

        reporter.log('Host audio test temp path is \"%s\"' % (sPathAudioOut));
        reporter.log('Host audio test output path is \"%s\"' % (sPathAudioTemp));
        reporter.log('Host audio test tag is \"%s\"' % (sTag));

        sVkatExe = self.getBinTool('vkat');

        reporter.log('Using VKAT on host at: \"%s\"' % (sVkatExe));

        # Build the base command line, exclude all tests by default.
        sArgs = '%s test -vvvv --mode host --tempdir %s --outdir %s -a' \
                % (sVkatExe, sPathAudioTemp, sPathAudioOut);

        # ... and extend it with wanted tests.
        sArgs += " " + sTests;

        fRc = True;

        reporter.testStart(sDesc);

        #
        # Let VKAT on the host run synchronously.
        #
        procVkat = subprocess.Popen(sArgs, \
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True);
        if procVkat:
            reporter.log('VKAT on host started');

            out, err = procVkat.communicate();
            rc = procVkat.poll();

            out = out.decode(sys.stdin.encoding);
            for line in out.split('\n'):
                reporter.log(line);

            err = err.decode(sys.stdin.encoding);
            for line in err.split('\n'):
                reporter.log(line);

            reporter.log('VKAT on host ended with exit code %d' % rc);
            if rc != 0:
                reporter.testFailure('VKAT on the host failed');
                fRc = False;
        else:
            reporter.testFailure('VKAT on the host failed to start');
            fRc = False;

        reporter.testDone();

        return fRc;

    def doTest(self, oTestVm, oSession, oTxsSession):
        """
        Executes the specified audio tests.
        """

        # Disable any OS-specific firewalls preventing VKAT / ATS to run.
        fRc = self.disableHstFirewall();
        fRc = self.disableGstFirewall(oTestVm, oTxsSession) and fRc;

        if not fRc:
            return False;

        # First try to kill any old VKAT / VBoxAudioTest processes lurking around on the host.
        # Might happen because of former (aborted) runs.
        self.killHstVkat();

        reporter.log("Active tests: %s" % (self.asTests,));

        fRc = self.startVkatOnGuest(oTestVm, oSession, oTxsSession);
        if fRc:
            #
            # Execute the tests using VKAT on the guest side (in guest mode).
            #
            if "guest_tone_playback" in self.asTests:
                fRc = self.runTests(oTestVm, oSession, oTxsSession, 'Guest audio playback', '-i0');
            if "guest_tone_recording" in self.asTests:
                fRc = fRc and self.runTests(oTestVm, oSession, oTxsSession, 'Guest audio recording', '-i1');

        #
        # Retrieve log files for diagnosis.
        #
        self.txsDownloadFiles(oSession, oTxsSession,
                              [ ( self.getGstVkatLogFilePath(oTestVm),
                                  'vkat-guest-daemonized-%s.log' % (oTestVm.sVmName,),),
                              ],
                              fIgnoreErrors = True);

        return fRc;

    def testOneVmConfig(self, oVM, oTestVm):
        """
        Runs tests using one specific VM config.
        """

        self.logVmInfo(oVM);

        if  oTestVm.isWindows() \
        and oTestVm.sKind in ('WindowsNT4', 'Windows2000'): # Too old for DirectSound and WASAPI backends.
            reporter.log('Audio testing skipped, not implemented/available for that OS yet.');
            return True;

        fRc = False;

        # Reconfigure the VM.
        oSession = self.openSession(oVM);
        if oSession is not None:
            # Set extra data.
            for sExtraData in self.asOptExtraData:
                sKey, sValue = sExtraData.split(':');
                reporter.log('Set extradata: %s => %s' % (sKey, sValue));
                fRc = oSession.setExtraData(sKey, sValue) and fRc;

            # Save the settings.
            fRc = fRc and oSession.saveSettings();
            fRc = oSession.close() and fRc;

        reporter.testStart('Waiting for TXS');
        oSession, oTxsSession = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName,
                                                                  fCdWait = True,
                                                                  cMsTimeout = 3 * 60 * 1000,
                                                                  sFileCdWait = '${OS/ARCH}/vkat${EXESUFF}');
        reporter.testDone();

        if  oSession is not None:
            self.addTask(oTxsSession);

            fRc = self.doTest(oTestVm, oSession, oTxsSession);

            # Cleanup.
            self.removeTask(oTxsSession);
            self.terminateVmBySession(oSession);

        return fRc;

    def onExit(self, iRc):
        """
        Exit handler for this test driver.
        """
        return vbox.TestDriver.onExit(self, iRc);

if __name__ == '__main__':
    sys.exit(tdAudioTest().main(sys.argv))
