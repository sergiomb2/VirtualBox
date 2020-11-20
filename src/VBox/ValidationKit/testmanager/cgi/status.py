#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
CGI - Administrator Web-UI.
"""

__copyright__ = \
"""
Copyright (C) 2012-2020 Oracle Corporation

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
import os
import sys
import datetime

# Only the main script needs to modify the path.
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from testmanager                        import config;
from testmanager.core.webservergluecgi  import WebServerGlueCgi;

from common                             import constants;
from testmanager.core.base              import TMExceptionBase;
from testmanager.core.db                import TMDatabaseConnection;



def how_many_days_in_month(year, month):
    def leap_year_check(year):
        if year % 4 == 0 and year % 100 != 0:
            return True
        if year % 100 == 0 and year % 400 == 0:
            return True
        return False

    month31 = (1, 3, 5, 7, 8, 10, 12)
    month30 = (4, 6, 9, 11)
    if month in month31:
        days = 31
    elif month in month30:
        days = 30
    else:
        if leap_year_check(year):
            days = 29
        else:
            days = 28
    return days


def target_date_from_time_span(cur_date, time_span_hours):
    cur_year = cur_date.year
    cur_month = cur_date.month
    cur_day = cur_date.day
    cur_hour = cur_date.hour
    if cur_hour >= time_span_hours:
        return cur_date.replace(hour=cur_hour-time_span_hours)
    if cur_day > 1:
        return cur_date.replace(day=cur_day-1,
                                hour=24+cur_hour-time_span_hours)
    if cur_month > 1:
        return cur_date.replace(month=cur_month-1,
                                day=how_many_days_in_month(cur_year, cur_month-1),
                                hour=24+cur_hour-time_span_hours)
    return cur_date.replace(year=cur_year-1,
                            month=12,
                            day=31,
                            hour=24+cur_hour-time_span_hours)


def find_test_duration(created):
    now = datetime.datetime.utcnow().replace(tzinfo=datetime.timezone.utc)
    diff = now - created
    days, seconds = diff.days, diff.seconds
    hours = days * 24 + seconds // 3600
    return hours


def testbox_data_processing(oDb):
    testboxes_dict = {}
    while True:
        line = oDb.fetchOne();
        if line is None:
            break;
        testbox_name = line[0]
        test_result = line[1]
        test_created = line[2]
        test_box_os = line[3]
        test_sched_group = line[4]
        testboxes_dict = dict_update(testboxes_dict, testbox_name, test_result)

        if "testbox_os" not in testboxes_dict[testbox_name]:
            testboxes_dict[testbox_name].update({"testbox_os": test_box_os})

        if "sched_group" not in testboxes_dict[testbox_name]:
            testboxes_dict[testbox_name].update({"sched_group": test_sched_group})
        elif test_sched_group not in testboxes_dict[testbox_name]["sched_group"]:
            testboxes_dict[testbox_name]["sched_group"] += "," + test_sched_group

        if test_result == "running":
            testboxes_dict[testbox_name].update({"hours_running": find_test_duration(test_created)})

    return testboxes_dict;


def os_results_separating(vb_dict, test_name, testbox_os, test_result):
    if testbox_os == "linux":
        dict_update(vb_dict, test_name + " / linux", test_result)
    elif testbox_os == "win":
        dict_update(vb_dict, test_name + " / windows", test_result)
    elif testbox_os == "darwin":
        dict_update(vb_dict, test_name + " / darwin", test_result)
    elif testbox_os == "solaris":
        dict_update(vb_dict, test_name + " / solaris", test_result)
#    else:
#        dict_update(vb_dict, test_name + " / other", test_result)


# const/immutable.
g_kdTestStatuses = {
    'running': 0,
    'success': 0,
    'skipped': 0,
    'bad-testbox': 0,
    'aborted': 0,
    'failure': 0,
    'timed-out': 0,
    'rebooted': 0,
}

def dict_update(target_dict, key_name, test_result):
    if key_name not in target_dict:
        target_dict.update({key_name: g_kdTestStatuses.copy()})
    if test_result in g_kdTestStatuses:
        target_dict[key_name][test_result] += 1
    return target_dict


def format_data(target_dict):
    content = ""
    for key in target_dict:
        if "hours_running" in target_dict[key].keys():
            content += "{};{};{} | running: {};{} | success: {} | skipped: {} | ".format(key,
                                                                                         target_dict[key]["testbox_os"],
                                                                                         target_dict[key]["sched_group"],
                                                                                         target_dict[key]["running"],
                                                                                         target_dict[key]["hours_running"],
                                                                                         target_dict[key]["success"],
                                                                                         target_dict[key]["skipped"],
                                                                                         )
        elif "testbox_os" in target_dict[key].keys():
            content += "{};{};{} | running: {} | success: {} | skipped: {} | ".format(key,
                                                                                      target_dict[key]["testbox_os"],
                                                                                      target_dict[key]["sched_group"],
                                                                                      target_dict[key]["running"],
                                                                                      target_dict[key]["success"],
                                                                                      target_dict[key]["skipped"],
                                                                                      )
        else:
            content += "{} | running: {} | success: {} | skipped: {} | ".format(key,
                                                                                target_dict[key]["running"],
                                                                                target_dict[key]["success"],
                                                                                target_dict[key]["skipped"],
                                                                                )
        content += "bad-testbox: {} | aborted: {} | failure: {} | ".format(
                                                                            target_dict[key]["bad-testbox"],
                                                                            target_dict[key]["aborted"],
                                                                            target_dict[key]["failure"],
                                                                            )
        content += "timed-out: {} | rebooted: {} | \n".format(
                                                             target_dict[key]["timed-out"],
                                                             target_dict[key]["rebooted"],
                                                             )
    return content

######

class StatusDispatcherException(TMExceptionBase):
    """
    Exception class for TestBoxController.
    """
    pass;                               # pylint: disable=unnecessary-pass


class StatusDispatcher(object): # pylint: disable=too-few-public-methods
    """
    Status dispatcher class.
    """


    def __init__(self, oSrvGlue):
        """
        Won't raise exceptions.
        """
        self._oSrvGlue          = oSrvGlue;
        self._sAction           = None; # _getStandardParams / dispatchRequest sets this later on.
        self._dParams           = None; # _getStandardParams / dispatchRequest sets this later on.
        self._asCheckedParams   = [];
        self._dActions          = \
        {
            'MagicMirrorTestResults': self._actionMagicMirrorTestResults,
            'MagicMirrorTestBoxes':   self._actionMagicMirrorTestBoxes,
        };

    def _getStringParam(self, sName, asValidValues = None, fStrip = False, sDefValue = None):
        """
        Gets a string parameter (stripped).

        Raises exception if not found and no default is provided, or if the
        value isn't found in asValidValues.
        """
        if sName not in self._dParams:
            if sDefValue is None:
                raise StatusDispatcherException('%s parameter %s is missing' % (self._sAction, sName));
            return sDefValue;
        sValue = self._dParams[sName];
        if fStrip:
            sValue = sValue.strip();

        if sName not in self._asCheckedParams:
            self._asCheckedParams.append(sName);

        if asValidValues is not None and sValue not in asValidValues:
            raise StatusDispatcherException('%s parameter %s value "%s" not in %s '
                                            % (self._sAction, sName, sValue, asValidValues));
        return sValue;

    def _getIntParam(self, sName, iMin = None, iMax = None, iDefValue = None):
        """
        Gets a string parameter.
        Raises exception if not found, not a valid integer, or if the value
        isn't in the range defined by iMin and iMax.
        """
        if sName not in self._dParams:
            if iDefValue is None:
                raise StatusDispatcherException('%s parameter %s is missing' % (self._sAction, sName));
            return iDefValue;
        sValue = self._dParams[sName];
        try:
            iValue = int(sValue, 0);
        except:
            raise StatusDispatcherException('%s parameter %s value "%s" cannot be convert to an integer'
                                            % (self._sAction, sName, sValue));
        if sName not in self._asCheckedParams:
            self._asCheckedParams.append(sName);

        if   (iMin is not None and iValue < iMin) \
          or (iMax is not None and iValue > iMax):
            raise StatusDispatcherException('%s parameter %s value %d is out of range [%s..%s]'
                                            % (self._sAction, sName, iValue, iMin, iMax));
        return iValue;

    def _checkForUnknownParameters(self):
        """
        Check if we've handled all parameters, raises exception if anything
        unknown was found.
        """

        if len(self._asCheckedParams) != len(self._dParams):
            sUnknownParams = '';
            for sKey in self._dParams:
                if sKey not in self._asCheckedParams:
                    sUnknownParams += ' ' + sKey + '=' + self._dParams[sKey];
            raise StatusDispatcherException('Unknown parameters: ' + sUnknownParams);

        return True;

    def _connectToDb(self):
        """
        Connects to the database.

        Returns (TMDatabaseConnection, (more later perhaps) ) on success.
        Returns (None, ) on failure after sending the box an appropriate response.
        May raise exception on DB error.
        """
        return (TMDatabaseConnection(self._oSrvGlue.dprint),);

    def _actionMagicMirrorTestBoxes(self):
        """
        Produces test result status for the magic mirror dashboard
        """

        #
        # Parse arguments and connect to the database.
        #
        cHoursBack = self._getIntParam('cHours', 1, 24*14, 12);
        self._checkForUnknownParameters();

        #
        # Get the data.
        #
        # Note! We're not joining on TestBoxesWithStrings.idTestBox =
        #       TestSets.idGenTestBox here because of indexes.  This is
        #       also more consistent with the rest of the query.
        # Note! The original SQL is slow because of the 'OR TestSets.tsDone'
        #       part, using AND and UNION is significatly faster because
        #       it matches the TestSetsGraphBoxIdx (index).
        #
        (oDb,) = self._connectToDb();
        if oDb is None:
            return False;

        #        oDb.execute('''
        #SELECT  TestBoxesWithStrings.sName,
        #        TestSets.enmStatus,
        #        TestSets.tsCreated,
        #        TestBoxesWithStrings.sOS,
        #        SchedGroupNames.sSchedGroupNames
        #FROM    (SELECT TestBoxesInSchedGroups.idTestBox AS idTestBox,
        #                STRING_AGG(SchedGroups.sName, ',') AS sSchedGroupNames
        #         FROM   TestBoxesInSchedGroups
        #         INNER JOIN SchedGroups
        #                 ON SchedGroups.idSchedGroup = TestBoxesInSchedGroups.idSchedGroup
        #         WHERE  TestBoxesInSchedGroups.tsExpire = 'infinity'::TIMESTAMP
        #            AND SchedGroups.tsExpire            = 'infinity'::TIMESTAMP
        #         GROUP BY TestBoxesInSchedGroups.idTestBox)
        #        AS SchedGroupNames,
        #        TestBoxesWithStrings
        #LEFT OUTER JOIN TestSets
        #             ON TestSets.idTestBox = TestBoxesWithStrings.idTestBox
        #            AND (   TestSets.tsCreated > (CURRENT_TIMESTAMP - '%s hours'::interval)
        #                 OR TestSets.tsDone IS NULL)
        #WHERE   TestBoxesWithStrings.tsExpire = 'infinity'::TIMESTAMP
        #  AND   SchedGroupNames.idTestBox = TestBoxesWithStrings.idTestBox
        #''', (cHoursBack,));
        oDb.execute('''
(   SELECT  TestBoxesWithStrings.sName,
            TestSets.enmStatus,
            TestSets.tsCreated,
            TestBoxesWithStrings.sOS,
            SchedGroupNames.sSchedGroupNames
    FROM    (
            SELECT TestBoxesInSchedGroups.idTestBox AS idTestBox,
            STRING_AGG(SchedGroups.sName, ',') AS sSchedGroupNames
            FROM   TestBoxesInSchedGroups
            INNER JOIN SchedGroups
                    ON SchedGroups.idSchedGroup = TestBoxesInSchedGroups.idSchedGroup
            WHERE   TestBoxesInSchedGroups.tsExpire = 'infinity'::TIMESTAMP
                AND SchedGroups.tsExpire            = 'infinity'::TIMESTAMP
            GROUP BY TestBoxesInSchedGroups.idTestBox
            ) AS SchedGroupNames,
            TestBoxesWithStrings
    LEFT OUTER JOIN TestSets
                 ON TestSets.idTestBox  = TestBoxesWithStrings.idTestBox
                AND TestSets.tsCreated >= (CURRENT_TIMESTAMP - '%s hours'::interval)
                AND TestSets.tsDone IS NOT NULL
    WHERE   TestBoxesWithStrings.tsExpire = 'infinity'::TIMESTAMP
      AND   SchedGroupNames.idTestBox = TestBoxesWithStrings.idTestBox
) UNION (
    SELECT  TestBoxesWithStrings.sName,
            TestSets.enmStatus,
            TestSets.tsCreated,
            TestBoxesWithStrings.sOS,
            SchedGroupNames.sSchedGroupNames
    FROM    (
            SELECT TestBoxesInSchedGroups.idTestBox AS idTestBox,
            STRING_AGG(SchedGroups.sName, ',') AS sSchedGroupNames
            FROM   TestBoxesInSchedGroups
            INNER JOIN SchedGroups
                    ON SchedGroups.idSchedGroup = TestBoxesInSchedGroups.idSchedGroup
            WHERE   TestBoxesInSchedGroups.tsExpire = 'infinity'::TIMESTAMP
                AND SchedGroups.tsExpire            = 'infinity'::TIMESTAMP
            GROUP BY TestBoxesInSchedGroups.idTestBox
            ) AS SchedGroupNames,
            TestBoxesWithStrings
    LEFT OUTER JOIN TestSets
                 ON TestSets.idTestBox  = TestBoxesWithStrings.idTestBox
                AND TestSets.tsCreated < (CURRENT_TIMESTAMP - '%s hours'::interval)
                AND TestSets.tsDone IS NULL
    WHERE   TestBoxesWithStrings.tsExpire = 'infinity'::TIMESTAMP
      AND   SchedGroupNames.idTestBox = TestBoxesWithStrings.idTestBox
)''', (cHoursBack, cHoursBack,));


        #
        # Process, format and output data.
        #
        dResult = testbox_data_processing(oDb);
        self._oSrvGlue.setContentType('text/plain');
        self._oSrvGlue.write(format_data(dResult));

        return True;

    def _actionMagicMirrorTestResults(self):
        """
        Produces test result status for the magic mirror dashboard
        """

        #
        # Parse arguments and connect to the database.
        #
        sBranch = self._getStringParam('sBranch');
        cHoursBack = self._getIntParam('cHours', 1, 24*14, 6); ## @todo why 6 hours here and 12 for test boxes?
        self._checkForUnknownParameters();

        #
        # Get the data.
        #
        # Note! These queries should be joining TestBoxesWithStrings and TestSets
        #       on idGenTestBox rather than on idTestBox and tsExpire=inf, but
        #       we don't have any index matching those.  So, we'll ignore tests
        #       performed by deleted testboxes for the present as that doesn't
        #       happen often and we want the ~1000x speedup.
        #
        (oDb,) = self._connectToDb();
        if oDb is None:
            return False;

        if sBranch == 'all':
            oDb.execute('''
SELECT  TestSets.enmStatus,
        TestCases.sName,
        TestBoxesWithStrings.sOS
FROM    TestSets
INNER JOIN TestCases
        ON TestCases.idGenTestCase         = TestCases.idGenTestCase
INNER JOIN TestBoxesWithStrings
        ON TestBoxesWithStrings.idTestBox  = TestSets.idTestBox
       AND TestBoxesWithStrings.tsExpire   = 'infinity'::TIMESTAMP
WHERE   TestSets.tsCreated                >= (CURRENT_TIMESTAMP - '%s hours'::interval)
''', (cHoursBack,));
        else:
            oDb.execute('''
SELECT  TestSets.enmStatus,
        TestCases.sName,
        TestBoxesWithStrings.sOS
FROM    TestSets
INNER JOIN BuildCategories
        ON BuildCategories.idBuildCategory = TestSets.idBuildCategory
       AND BuildCategories.sBranch         = '%s'
INNER JOIN TestCases
        ON TestCases.idGenTestCase         = TestSets.idGenTestCase
INNER JOIN TestBoxesWithStrings
        ON TestBoxesWithStrings.idTestBox  = TestSets.idTestBox
       AND TestBoxesWithStrings.tsExpire   = 'infinity'::TIMESTAMP
WHERE   TestSets.tsCreated                >= (CURRENT_TIMESTAMP - '%s hours'::interval)
''', (sBranch, cHoursBack,));

        # Process the data
        dResult = {};
        while True:
            aoRow = oDb.fetchOne();
            if aoRow is None:
                break;
            os_results_separating(dResult, aoRow[1], aoRow[2], aoRow[0])  # save all test results

        # Format and output it.
        self._oSrvGlue.setContentType('text/plain');
        self._oSrvGlue.write(format_data(dResult));

        return True;

    def _getStandardParams(self, dParams):
        """
        Gets the standard parameters and validates them.

        The parameters are returned as a tuple: sAction, (more later, maybe)
        Note! the sTextBoxId can be None if it's a SIGNON request.

        Raises StatusDispatcherException on invalid input.
        """
        #
        # Get the action parameter and validate it.
        #
        if constants.tbreq.ALL_PARAM_ACTION not in dParams:
            raise StatusDispatcherException('No "%s" parameter in request (params: %s)'
                                            % (constants.tbreq.ALL_PARAM_ACTION, dParams,));
        sAction = dParams[constants.tbreq.ALL_PARAM_ACTION];

        if sAction not in self._dActions:
            raise StatusDispatcherException('Unknown action "%s" in request (params: %s; action: %s)'
                                            % (sAction, dParams, self._dActions));
        #
        # Update the list of checked parameters.
        #
        self._asCheckedParams.extend([constants.tbreq.ALL_PARAM_ACTION,]);

        return (sAction,);

    def dispatchRequest(self):
        """
        Dispatches the incoming request.

        Will raise StatusDispatcherException on failure.
        """

        #
        # Must be a GET request.
        #
        try:
            sMethod = self._oSrvGlue.getMethod();
        except Exception as oXcpt:
            raise StatusDispatcherException('Error retriving request method: %s' % (oXcpt,));
        if sMethod != 'GET':
            raise StatusDispatcherException('Error expected POST request not "%s"' % (sMethod,));

        #
        # Get the parameters and checks for duplicates.
        #
        try:
            dParams = self._oSrvGlue.getParameters();
        except Exception as oXcpt:
            raise StatusDispatcherException('Error retriving parameters: %s' % (oXcpt,));
        for sKey in dParams.keys():
            if len(dParams[sKey]) > 1:
                raise StatusDispatcherException('Parameter "%s" is given multiple times: %s' % (sKey, dParams[sKey]));
            dParams[sKey] = dParams[sKey][0];
        self._dParams = dParams;

        #
        # Get+validate the standard action parameters and dispatch the request.
        #
        (self._sAction, ) = self._getStandardParams(dParams);
        return self._dActions[self._sAction]();


def main():
    """
    Main function a la C/C++. Returns exit code.
    """

    oSrvGlue = WebServerGlueCgi(g_ksValidationKitDir, fHtmlOutput = False);
    try:
        oDisp = StatusDispatcher(oSrvGlue);
        oDisp.dispatchRequest();
        oSrvGlue.flush();
    except Exception as oXcpt:
        return oSrvGlue.errorPage('Internal error: %s' % (str(oXcpt),), sys.exc_info());

    return 0;

if __name__ == '__main__':
    if config.g_kfProfileAdmin:
        from testmanager.debug import cgiprofiling;
        sys.exit(cgiprofiling.profileIt(main));
    else:
        sys.exit(main());

