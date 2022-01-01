/* $Id$ */
/** @file
 * Linux .r0 wrapper module template.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define IPRT_WITHOUT_EFLAGS_AC_PRESERVING
#include "the-linux-kernel.h"

#include "version-generated.h"
#include "product-generated.h"
#include "revision-generated.h"

#include <VBox/sup.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @def WRAPPED_MODULE_FLAGS
 * SUPLDRWRAPPEDMODULE_F_XXX or 0.          Default: 0 */
#ifndef WRAPPED_MODULE_FLAGS
# define WRAPPED_MODULE_FLAGS               0
#endif
/** @def WRAPPED_MODULE_INIT
 * The module init function or NULL.        Default: ModuleInit */
#ifndef WRAPPED_MODULE_INIT
# define WRAPPED_MODULE_INIT                ModuleInit
#endif
/** @def WRAPPED_MODULE_TERM
 * The module termination function or NULL. Default: ModuleTerm */
#ifndef WRAPPED_MODULE_TERM
# define WRAPPED_MODULE_TERM                ModuleTerm
#endif
/** @def WRAPPED_MODULE_SRV_REQ_HANDLER
 * The service request handler function.    Default: NULL */
#ifndef WRAPPED_MODULE_SRV_REQ_HANDLER
# define WRAPPED_MODULE_SRV_REQ_HANDLER     NULL
#endif
/** @def WRAPPED_MODULE_VMMR0_ENTRY_FAST
 * The VMMR0 fast entry point.              Default: NULL */
#ifndef WRAPPED_MODULE_VMMR0_ENTRY_FAST
# define WRAPPED_MODULE_VMMR0_ENTRY_FAST    NULL
#endif
/** @def WRAPPED_MODULE_VMMR0_ENTRY_EX
 * The VMMR0 extended entry point.          Default: NULL */
#ifndef WRAPPED_MODULE_VMMR0_ENTRY_EX
# define WRAPPED_MODULE_VMMR0_ENTRY_EX      NULL
#endif
/** @def WRAPPED_MODULE_SRV_REQ_HANDLER
 * The service request handler function.    Default: NULL */
#ifndef WRAPPED_MODULE_SRV_REQ_HANDLER
# define WRAPPED_MODULE_SRV_REQ_HANDLER     NULL
#endif

#ifdef DOXYGEN_RUNNING
/** @def WRAPPED_MODULE_LINUX_EXPORTS
 * Define to enabled linux exports.  (Needed for VMMR0.r0 only at present.) */
# define WRAPPED_MODULE_LINUX_EXPORTS
#endif
#ifdef DOXYGEN_RUNNING
/** @def WRAPPED_MODULE_LICENSE_PROPRIETARY
 * Define to select proprietary license instead of GPL. */
# define WRAPPED_MODULE_LICENSE_PROPRIETARY
#endif
#ifdef DOXYGEN_RUNNING
/** @def WRAPPED_MODULE_SYMBOL_INCLUDE
 * The include with SYMBOL_ENTRY() invocations for all exported symbols.  */
# define WRAPPED_MODULE_SYMBOL_INCLUDE      "iprt/cdefs.h"
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int  __init VBoxWrapperModInit(void);
static void __exit VBoxWrapperModUnload(void);


/*
 * Prototype the symbols:
 */
#undef RT_MANGLER
#define RT_MANGLER(a_Name)      a_Name /* No mangling */
#define SYMBOL_ENTRY(a_Name)    extern FNRT a_Name;
#include WRAPPED_MODULE_SYMBOL_INCLUDE
#undef  SYMBOL_ENTRY

/*
 * Export the symbols linux style:
 */
#ifdef WRAPPED_MODULE_LINUX_EXPORTS
# define SYMBOL_ENTRY(a_Name)    EXPORT_SYMBOL(a_Name);
# include WRAPPED_MODULE_SYMBOL_INCLUDE
# undef  SYMBOL_ENTRY
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
extern char vboxr0mod_start[];  /**< start of text in the .r0 module. */
extern char vboxr0mod_end[];    /**< end of bss in the .r0 module. */

/** The symbol table. */
static SUPLDRWRAPMODSYMBOL const g_aSymbols[] =
{
#define SYMBOL_ENTRY(a_Name)    { #a_Name, &a_Name },
#include WRAPPED_MODULE_SYMBOL_INCLUDE
#undef  SYMBOL_ENTRY
};

/** Wrapped module registration info. */
static SUPLDRWRAPPEDMODULE const g_WrappedMod =
{
    /* .uMagic = */             SUPLDRWRAPPEDMODULE_MAGIC,
    /* .uVersion = */           SUPLDRWRAPPEDMODULE_VERSION,
    /* .fFlags = */             WRAPPED_MODULE_FLAGS,
    /* .pvImageStart = */       &vboxr0mod_start[0],
    /* .pvImageEnd = */         &vboxr0mod_end[0],

    /* .pfnModuleInit = */      WRAPPED_MODULE_INIT,
    /* .pfnModuleTerm = */      WRAPPED_MODULE_TERM,
    /* .pfnVMMR0EntryFast = */  WRAPPED_MODULE_VMMR0_ENTRY_FAST,
    /* .pfnVMMR0EntryEx = */    WRAPPED_MODULE_VMMR0_ENTRY_EX,
    /* .pfnSrvReqHandler = */   (PFNSUPR0SERVICEREQHANDLER)WRAPPED_MODULE_SRV_REQ_HANDLER,

    /* .apSymbols = */          g_aSymbols,
    /* .cSymbols = */           RT_ELEMENTS(g_aSymbols),

    /* .szName = */             WRAPPED_MODULE_NAME,
    /* .uEndMagic = */          SUPLDRWRAPPEDMODULE_MAGIC,
};

/** The wrapped module handle. */
static void *g_hWrappedRegistration = NULL;


/**
 * Initialize module.
 *
 * @returns appropriate status code.
 */
static int __init VBoxWrapperModInit(void)
{
    /*printk("vboxwrap/" WRAPPED_MODULE_NAME ": VBoxWrapperModInit\n");*/
    int rc = SUPDrvLinuxLdrRegisterWrappedModule(&g_WrappedMod, KBUILD_MODNAME, &g_hWrappedRegistration);
    if (rc == 0)
        return 0;
    printk("vboxwrap/" WRAPPED_MODULE_NAME ": SUPDrvLinuxRegisterWrappedModule failed: %d\n", rc);
    return -EINVAL;
}


/**
 * Unload the module.
 */
static void __exit VBoxWrapperModUnload(void)
{
    /*printk("vboxwrap/" WRAPPED_MODULE_NAME ": VBoxWrapperModUnload\n");*/
    SUPDrvLinuxLdrDeregisterWrappedModule(&g_WrappedMod, &g_hWrappedRegistration);
}

module_init(VBoxWrapperModInit);
module_exit(VBoxWrapperModUnload);

MODULE_AUTHOR(VBOX_VENDOR);
MODULE_DESCRIPTION(VBOX_PRODUCT " - " WRAPPED_MODULE_NAME);
#ifndef WRAPPED_MODULE_LICENSE_PROPRIETARY
MODULE_LICENSE("GPL");
#else
MODULE_LICENSE("Proprietary");
#endif
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING " r" RT_XSTR(VBOX_SVN_REV));
#endif

