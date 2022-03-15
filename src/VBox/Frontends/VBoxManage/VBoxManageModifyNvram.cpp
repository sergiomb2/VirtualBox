/* $Id$ */
/** @file
 * VBoxManage - The nvram control related commands.
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

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/errcore.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/ctype.h>
#include <iprt/stream.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/uuid.h>
#include <VBox/log.h>

#include "VBoxManage.h"
using namespace com;

DECLARE_TRANSLATION_CONTEXT(Nvram);

// funcs
///////////////////////////////////////////////////////////////////////////////


/**
 * Handles the 'modifynvram myvm inituefivarstore' sub-command.
 * @returns Exit code.
 * @param   a               The handler argument package.
 * @param   nvram           Reference to the NVRAM store interface.
 */
static RTEXITCODE handleModifyNvramInitUefiVarStore(HandlerArg *a, ComPtr<INvramStore> &nvramStore)
{
    RT_NOREF(a);

    CHECK_ERROR2I_RET(nvramStore, InitUefiVariableStore(0 /*aSize*/), RTEXITCODE_FAILURE);
    return RTEXITCODE_SUCCESS;
}


/**
 * Handles the 'modifynvram myvm enrollmssignatures' sub-command.
 * @returns Exit code.
 * @param   a               The handler argument package.
 * @param   nvram           Reference to the NVRAM store interface.
 */
static RTEXITCODE handleModifyNvramEnrollMsSignatures(HandlerArg *a, ComPtr<INvramStore> &nvramStore)
{
    RT_NOREF(a);

    ComPtr<IUefiVariableStore> uefiVarStore;
    CHECK_ERROR2I_RET(nvramStore, COMGETTER(UefiVariableStore)(uefiVarStore.asOutParam()), RTEXITCODE_FAILURE);

    CHECK_ERROR2I_RET(uefiVarStore, EnrollDefaultMsSignatures(), RTEXITCODE_FAILURE);
    return RTEXITCODE_SUCCESS;
}


/**
 * Handles the 'modifynvram myvm enrollpk' sub-command.
 * @returns Exit code.
 * @param   a               The handler argument package.
 * @param   nvram           Reference to the NVRAM store interface.
 */
static RTEXITCODE handleModifyNvramEnrollPlatformKey(HandlerArg *a, ComPtr<INvramStore> &nvramStore)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        /* common options */
        { "--platform-key", 'p', RTGETOPT_REQ_STRING },
        { "--owner-uuid",   'f', RTGETOPT_REQ_STRING }
    };

    const char *pszPlatformKey = NULL;
    const char *pszOwnerUuid = NULL;

    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, a->argc - 2, &a->argv[2], s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'p':
                pszPlatformKey = ValueUnion.psz;
                break;
            case 'f':
                pszOwnerUuid = ValueUnion.psz;
                break;
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    if (!pszPlatformKey)
        return errorSyntax(Nvram::tr("No platform key file path was given to \"enrollpk\""));
    if (!pszOwnerUuid)
        return errorSyntax(Nvram::tr("No owner UUID was given to \"enrollpk\""));

    RTFILE hPkFile;
    vrc = RTFileOpen(&hPkFile, pszPlatformKey, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_SUCCESS(vrc))
    {
        uint64_t cbSize;
        vrc = RTFileQuerySize(hPkFile, &cbSize);
        if (RT_SUCCESS(vrc))
        {
            if (cbSize <= _32K)
            {
                SafeArray<BYTE> aPk((size_t)cbSize);
                vrc = RTFileRead(hPkFile, aPk.raw(), (size_t)cbSize, NULL);
                if (RT_SUCCESS(vrc))
                {
                    RTFileClose(hPkFile);

                    ComPtr<IUefiVariableStore> uefiVarStore;
                    CHECK_ERROR2I_RET(nvramStore, COMGETTER(UefiVariableStore)(uefiVarStore.asOutParam()), RTEXITCODE_FAILURE);
                    CHECK_ERROR2I_RET(uefiVarStore, EnrollPlatformKey(ComSafeArrayAsInParam(aPk), Bstr(pszOwnerUuid).raw()), RTEXITCODE_FAILURE);
                    return RTEXITCODE_SUCCESS;
                }
                else
                    RTMsgError(Nvram::tr("Cannot read contents of file \"%s\": %Rrc"), pszPlatformKey, vrc);
            }
            else
                RTMsgError(Nvram::tr("File \"%s\" is bigger than 32KByte"), pszPlatformKey);
        }
        else
            RTMsgError(Nvram::tr("Cannot get size of file \"%s\": %Rrc"), pszPlatformKey, vrc);

        RTFileClose(hPkFile);
    }
    else
        RTMsgError(Nvram::tr("Cannot open file \"%s\": %Rrc"), pszPlatformKey, vrc);

    return RTEXITCODE_FAILURE;
}


/**
 * Handles the 'modifynvram myvm enrollorclpk' sub-command.
 * @returns Exit code.
 * @param   a               The handler argument package.
 * @param   nvram           Reference to the NVRAM store interface.
 */
static RTEXITCODE handleModifyNvramEnrollOraclePlatformKey(HandlerArg *a, ComPtr<INvramStore> &nvramStore)
{
    RT_NOREF(a);

    ComPtr<IUefiVariableStore> uefiVarStore;
    CHECK_ERROR2I_RET(nvramStore, COMGETTER(UefiVariableStore)(uefiVarStore.asOutParam()), RTEXITCODE_FAILURE);

    CHECK_ERROR2I_RET(uefiVarStore, EnrollOraclePlatformKey(), RTEXITCODE_FAILURE);
    return RTEXITCODE_SUCCESS;
}


/**
 * Handles the 'modifynvram myvm listvars' sub-command.
 * @returns Exit code.
 * @param   a               The handler argument package.
 * @param   nvram           Reference to the NVRAM store interface.
 */
static RTEXITCODE handleModifyNvramListUefiVars(HandlerArg *a, ComPtr<INvramStore> &nvramStore)
{
    RT_NOREF(a);

    ComPtr<IUefiVariableStore> uefiVarStore;
    CHECK_ERROR2I_RET(nvramStore, COMGETTER(UefiVariableStore)(uefiVarStore.asOutParam()), RTEXITCODE_FAILURE);

    com::SafeArray<BSTR> aNames;
    com::SafeArray<BSTR> aOwnerGuids;
    CHECK_ERROR2I_RET(uefiVarStore, QueryVariables(ComSafeArrayAsOutParam(aNames), ComSafeArrayAsOutParam(aOwnerGuids)), RTEXITCODE_FAILURE);
    for (size_t i = 0; i < aNames.size(); i++)
    {
        Bstr strName      = aNames[i];
        Bstr strOwnerGuid = aOwnerGuids[i];

        RTPrintf("%-32ls {%ls}\n", strName.raw(), strOwnerGuid.raw());
    }

    return RTEXITCODE_SUCCESS;
}


/**
 * Handles the 'modifynvram myvm queryvar' sub-command.
 * @returns Exit code.
 * @param   a               The handler argument package.
 * @param   nvram           Reference to the NVRAM store interface.
 */
static RTEXITCODE handleModifyNvramQueryUefiVar(HandlerArg *a, ComPtr<INvramStore> &nvramStore)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        /* common options */
        { "--name",       'n', RTGETOPT_REQ_STRING },
        { "--filename",   'f', RTGETOPT_REQ_STRING }
    };

    const char *pszVarName = NULL;
    const char *pszVarDataFilename = NULL;

    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, a->argc - 2, &a->argv[2], s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'n':
                pszVarName = ValueUnion.psz;
                break;
            case 'f':
                pszVarDataFilename = ValueUnion.psz;
                break;
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    if (!pszVarName)
        return errorSyntax(Nvram::tr("No variable name was given to \"queryvar\""));

    ComPtr<IUefiVariableStore> uefiVarStore;
    CHECK_ERROR2I_RET(nvramStore, COMGETTER(UefiVariableStore)(uefiVarStore.asOutParam()), RTEXITCODE_FAILURE);

    Bstr strOwnerGuid;
    com::SafeArray<UefiVariableAttributes_T> aVarAttrs;
    com::SafeArray<BYTE> aData;
    CHECK_ERROR2I_RET(uefiVarStore, QueryVariableByName(Bstr(pszVarName).raw(), strOwnerGuid.asOutParam(),
                                                        ComSafeArrayAsOutParam(aVarAttrs), ComSafeArrayAsOutParam(aData)),
                      RTEXITCODE_FAILURE);

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    if (!pszVarDataFilename)
    {
        RTPrintf("%s {%ls}:\n"
                 "%.*Rhxd\n", pszVarName, strOwnerGuid.raw(), aData.size(), aData.raw());
    }
    else
    {
        /* Just write the data to the file. */
        RTFILE hFile = NIL_RTFILE;
        vrc = RTFileOpen(&hFile, pszVarDataFilename, RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(vrc))
        {
            vrc = RTFileWrite(hFile, aData.raw(), aData.size(), NULL /*pcbWritten*/);
            if (RT_FAILURE(vrc))
                rcExit = RTMsgErrorExitFailure(Nvram::tr("Error writing to '%s': %Rrc"), pszVarDataFilename, vrc);

            RTFileClose(hFile);
        }
        else
           rcExit = RTMsgErrorExitFailure(Nvram::tr("Error opening '%s': %Rrc"), pszVarDataFilename, vrc);
    }

    return rcExit;
}


/**
 * Handles the 'modifynvram myvm deletevar' sub-command.
 * @returns Exit code.
 * @param   a               The handler argument package.
 * @param   nvram           Reference to the NVRAM store interface.
 */
static RTEXITCODE handleModifyNvramDeleteUefiVar(HandlerArg *a, ComPtr<INvramStore> &nvramStore)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        /* common options */
        { "--name",       'n', RTGETOPT_REQ_STRING },
        { "--owner-uuid", 'f', RTGETOPT_REQ_STRING }
    };

    const char *pszVarName = NULL;
    const char *pszOwnerUuid = NULL;

    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, a->argc - 2, &a->argv[2], s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'n':
                pszVarName = ValueUnion.psz;
                break;
            case 'f':
                pszOwnerUuid = ValueUnion.psz;
                break;
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    if (!pszVarName)
        return errorSyntax(Nvram::tr("No variable name was given to \"deletevar\""));
    if (!pszOwnerUuid)
        return errorSyntax(Nvram::tr("No owner UUID was given to \"deletevar\""));

    ComPtr<IUefiVariableStore> uefiVarStore;
    CHECK_ERROR2I_RET(nvramStore, COMGETTER(UefiVariableStore)(uefiVarStore.asOutParam()), RTEXITCODE_FAILURE);
    CHECK_ERROR2I_RET(uefiVarStore, DeleteVariable(Bstr(pszVarName).raw(), Bstr(pszOwnerUuid).raw()), RTEXITCODE_FAILURE);

    return RTEXITCODE_SUCCESS;
}


/**
 * Handles the 'modifynvram myvm changevar' sub-command.
 * @returns Exit code.
 * @param   a               The handler argument package.
 * @param   nvram           Reference to the NVRAM store interface.
 */
static RTEXITCODE handleModifyNvramChangeUefiVar(HandlerArg *a, ComPtr<INvramStore> &nvramStore)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        /* common options */
        { "--name",       'n', RTGETOPT_REQ_STRING },
        { "--filename",   'f', RTGETOPT_REQ_STRING }
    };

    const char *pszVarName = NULL;
    const char *pszVarDataFilename = NULL;

    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, a->argc - 2, &a->argv[2], s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'n':
                pszVarName = ValueUnion.psz;
                break;
            case 'f':
                pszVarDataFilename = ValueUnion.psz;
                break;
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    if (!pszVarName)
        return errorSyntax(Nvram::tr("No variable name was given to \"changevar\""));
    if (!pszVarDataFilename)
        return errorSyntax(Nvram::tr("No variable data filename was given to \"changevar\""));

    RTFILE hFile = NIL_RTFILE;
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    vrc = RTFileOpen(&hFile, pszVarDataFilename, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(vrc))
    {
        uint64_t cbFile = 0;
        vrc = RTFileQuerySize(hFile, &cbFile);
        if (RT_SUCCESS(vrc))
        {
            com::SafeArray<BYTE> aData;
            aData.resize(cbFile);

            vrc = RTFileRead(hFile, aData.raw(), aData.size(), NULL /*pcbRead*/);
            RTFileClose(hFile);

            if (RT_SUCCESS(vrc))
            {
                ComPtr<IUefiVariableStore> uefiVarStore;
                CHECK_ERROR2I_RET(nvramStore, COMGETTER(UefiVariableStore)(uefiVarStore.asOutParam()), RTEXITCODE_FAILURE);
                CHECK_ERROR2I_RET(uefiVarStore, ChangeVariable(Bstr(pszVarName).raw(), ComSafeArrayAsInParam(aData)), RTEXITCODE_FAILURE);
            }
            else
                rcExit = RTMsgErrorExitFailure(Nvram::tr("Error reading from '%s': %Rrc"), pszVarDataFilename, vrc);
        }
    }
    else
       rcExit = RTMsgErrorExitFailure(Nvram::tr("Error opening '%s': %Rrc"), pszVarDataFilename, vrc);

    return rcExit;
}


/**
 * Handles the 'modifynvram' command.
 * @returns Exit code.
 * @param   a               The handler argument package.
 */
RTEXITCODE handleModifyNvram(HandlerArg *a)
{
    HRESULT rc = S_OK;
    ComPtr<IMachine> machine;
    ComPtr<INvramStore> nvramStore;

    if (a->argc < 2)
        return errorNoSubcommand();

    /* try to find the given machine */
    CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()), RTEXITCODE_FAILURE);

    /* open a session for the VM (new or shared) */
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Write), RTEXITCODE_FAILURE);

    /* get the mutable session machine */
    a->session->COMGETTER(Machine)(machine.asOutParam());
    rc = machine->COMGETTER(NonVolatileStore)(nvramStore.asOutParam());
    if (FAILED(rc)) goto leave;

    if (!strcmp(a->argv[1], "inituefivarstore"))
    {
        setCurrentSubcommand(HELP_SCOPE_MODIFYNVRAM_INITUEFIVARSTORE);
        rc = handleModifyNvramInitUefiVarStore(a, nvramStore) == RTEXITCODE_SUCCESS ? S_OK : E_FAIL;
    }
    else if (!strcmp(a->argv[1], "enrollmssignatures"))
    {
        setCurrentSubcommand(HELP_SCOPE_MODIFYNVRAM_ENROLLMSSIGNATURES);
        rc = handleModifyNvramEnrollMsSignatures(a, nvramStore) == RTEXITCODE_SUCCESS ? S_OK : E_FAIL;
    }
    else if (!strcmp(a->argv[1], "enrollpk"))
    {
        setCurrentSubcommand(HELP_SCOPE_MODIFYNVRAM_ENROLLPK);
        rc = handleModifyNvramEnrollPlatformKey(a, nvramStore) == RTEXITCODE_SUCCESS ? S_OK : E_FAIL;
    }
    else if (!strcmp(a->argv[1], "enrollorclpk"))
    {
        setCurrentSubcommand(HELP_SCOPE_MODIFYNVRAM_ENROLLORCLPK);
        rc = handleModifyNvramEnrollOraclePlatformKey(a, nvramStore) == RTEXITCODE_SUCCESS ? S_OK : E_FAIL;
    }
    else if (!strcmp(a->argv[1], "listvars"))
    {
        setCurrentSubcommand(HELP_SCOPE_MODIFYNVRAM_LISTVARS);
        rc = handleModifyNvramListUefiVars(a, nvramStore) == RTEXITCODE_SUCCESS ? S_OK : E_FAIL;
    }
    else if (!strcmp(a->argv[1], "queryvar"))
    {
        setCurrentSubcommand(HELP_SCOPE_MODIFYNVRAM_QUERYVAR);
        rc = handleModifyNvramQueryUefiVar(a, nvramStore) == RTEXITCODE_SUCCESS ? S_OK : E_FAIL;
    }
    else if (!strcmp(a->argv[1], "deletevar"))
    {
        setCurrentSubcommand(HELP_SCOPE_MODIFYNVRAM_DELETEVAR);
        rc = handleModifyNvramDeleteUefiVar(a, nvramStore) == RTEXITCODE_SUCCESS ? S_OK : E_FAIL;
    }
    else if (!strcmp(a->argv[1], "changevar"))
    {
        setCurrentSubcommand(HELP_SCOPE_MODIFYNVRAM_CHANGEVAR);
        rc = handleModifyNvramChangeUefiVar(a, nvramStore) == RTEXITCODE_SUCCESS ? S_OK : E_FAIL;
    }
    else
        return errorUnknownSubcommand(a->argv[0]);

    /* commit changes */
    if (SUCCEEDED(rc))
        CHECK_ERROR(machine, SaveSettings());

leave:
    /* it's important to always close sessions */
    a->session->UnlockMachine();

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}
