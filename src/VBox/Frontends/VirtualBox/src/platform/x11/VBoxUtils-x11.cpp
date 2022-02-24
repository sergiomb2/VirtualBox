/* $Id$ */
/** @file
 * VBox Qt GUI - Declarations of utility classes and functions for handling X11 specific tasks.
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

/* Qt includes: */
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include <QX11Info>
#include <QWidget>

/* GUI includes: */
#include "VBoxUtils-x11.h"

/* Other VBox includes: */
#include <iprt/assert.h>
#include <VBox/log.h>

/* Other includes: */
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/dpms.h>

/// @todo is it required still?
// WORKAROUND:
// rhel3 build hack
//RT_C_DECLS_BEGIN
//#include <X11/Xlib.h>
//#undef BOOL /* VBox/com/defs.h conflict */
//RT_C_DECLS_END


bool NativeWindowSubsystem::X11IsCompositingManagerRunning()
{
    /* For each screen it manage, compositing manager MUST acquire ownership
     * of a selection named _NET_WM_CM_Sn, where n is the screen number. */
    Display *pDisplay = QX11Info::display();
    Atom atom_property_name = XInternAtom(pDisplay, "_NET_WM_CM_S0", True);
    return XGetSelectionOwner(pDisplay, atom_property_name);
}

X11WMType NativeWindowSubsystem::X11WindowManagerType()
{
    /* Ask if root-window supports check for WM name: */
    Display *pDisplay = QX11Info::display();
    Atom atom_property_name;
    Atom atom_returned_type;
    int iReturnedFormat;
    unsigned long ulReturnedItemCount;
    unsigned long ulDummy;
    unsigned char *pcData = 0;
    X11WMType wmType = X11WMType_Unknown;
    atom_property_name = XInternAtom(pDisplay, "_NET_SUPPORTING_WM_CHECK", True);
    if (XGetWindowProperty(pDisplay, QX11Info::appRootWindow(), atom_property_name,
                           0, 512, False, XA_WINDOW, &atom_returned_type,
                           &iReturnedFormat, &ulReturnedItemCount, &ulDummy, &pcData) == Success)
    {
        Window WMWindow = None;
        if (atom_returned_type == XA_WINDOW && iReturnedFormat == 32)
            WMWindow = *((Window*)pcData);
        if (pcData)
            XFree(pcData);
        if (WMWindow != None)
        {
            /* Ask root-window for WM name: */
            atom_property_name = XInternAtom(pDisplay, "_NET_WM_NAME", True);
            Atom utf8Atom = XInternAtom(pDisplay, "UTF8_STRING", True);
            if (XGetWindowProperty(pDisplay, WMWindow, atom_property_name,
                                   0, 512, False, utf8Atom, &atom_returned_type,
                                   &iReturnedFormat, &ulReturnedItemCount, &ulDummy, &pcData) == Success)
            {
                if (QString((const char*)pcData).contains("Compiz", Qt::CaseInsensitive))
                    wmType = X11WMType_Compiz;
                else
                if (QString((const char*)pcData).contains("GNOME Shell", Qt::CaseInsensitive))
                    wmType = X11WMType_GNOMEShell;
                else
                if (QString((const char*)pcData).contains("KWin", Qt::CaseInsensitive))
                    wmType = X11WMType_KWin;
                else
                if (QString((const char*)pcData).contains("Metacity", Qt::CaseInsensitive))
                    wmType = X11WMType_Metacity;
                else
                if (QString((const char*)pcData).contains("Mutter", Qt::CaseInsensitive))
                    wmType = X11WMType_Mutter;
                else
                if (QString((const char*)pcData).contains("Xfwm4", Qt::CaseInsensitive))
                    wmType = X11WMType_Xfwm4;
                if (pcData)
                    XFree(pcData);
            }
        }
    }
    return wmType;
}

#if 0 // unused for now?
static int  gX11ScreenSaverTimeout;
static BOOL gX11ScreenSaverDpmsAvailable;
static BOOL gX11DpmsState;

void NativeWindowSubsystem::X11ScreenSaverSettingsInit()
{
    /* Init screen-save availability: */
    Display *pDisplay = QX11Info::display();
    int dummy;
    gX11ScreenSaverDpmsAvailable = DPMSQueryExtension(pDisplay, &dummy, &dummy);
}

void NativeWindowSubsystem::X11ScreenSaverSettingsSave()
{
    /* Actually this is a big mess. By default the libSDL disables the screen saver
     * during the SDL_InitSubSystem() call and restores the saved settings during
     * the SDL_QuitSubSystem() call. This mechanism can be disabled by setting the
     * environment variable SDL_VIDEO_ALLOW_SCREENSAVER to 1. However, there is a
     * known bug in the Debian libSDL: If this environment variable is set, the
     * screen saver is still disabled but the old state is not restored during
     * SDL_QuitSubSystem()! So the only solution to overcome this problem is to
     * save and restore the state prior and after each of these function calls. */

    Display *pDisplay = QX11Info::display();
    int dummy;
    CARD16 dummy2;
    XGetScreenSaver(pDisplay, &gX11ScreenSaverTimeout, &dummy, &dummy, &dummy);
    if (gX11ScreenSaverDpmsAvailable)
        DPMSInfo(pDisplay, &dummy2, &gX11DpmsState);
}

void NativeWindowSubsystem::X11ScreenSaverSettingsRestore()
{
    /* Restore screen-saver settings: */
    Display *pDisplay = QX11Info::display();
    int iTimeout, iInterval, iPreferBlank, iAllowExp;
    XGetScreenSaver(pDisplay, &iTimeout, &iInterval, &iPreferBlank, &iAllowExp);
    iTimeout = gX11ScreenSaverTimeout;
    XSetScreenSaver(pDisplay, iTimeout, iInterval, iPreferBlank, iAllowExp);
    if (gX11DpmsState && gX11ScreenSaverDpmsAvailable)
        DPMSEnable(pDisplay);
}
#endif // unused for now?

bool NativeWindowSubsystem::X11CheckExtension(const char *pExtensionName)
{
    /* Check extension: */
    Display *pDisplay = QX11Info::display();
    int major_opcode;
    int first_event;
    int first_error;
    return XQueryExtension(pDisplay, pExtensionName, &major_opcode, &first_event, &first_error);
}

bool X11CheckDBusConnection(const QDBusConnection &connection)
{
    if (!connection.isConnected())
    {
        const QDBusError lastError = connection.lastError();
        if (lastError.isValid())
        {
            LogRel(("QDBus error. Could not connect to D-Bus server: %s: %s\n",
                    lastError.name().toUtf8().constData(),
                    lastError.message().toUtf8().constData()));
        }
        else
            LogRel(("QDBus error. Could not connect to D-Bus server: Unable to load dbus libraries\n"));
        return false;
    }
    return true;
}

QStringList X11FindDBusScreenSaverServices(const QDBusConnection &connection)
{
    QStringList serviceNames;

    QDBusReply<QStringList> replyr = connection.interface()->registeredServiceNames();
    if (!replyr.isValid())
    {
        const QDBusError replyError = replyr.error();
        LogRel(("QDBus error. Could not query registered service names %s %s",
                replyError.name().toUtf8().constData(), replyError.message().toUtf8().constData()));
        return serviceNames;
    }

    for (int i = 0; i < replyr.value().size(); ++i)
    {
        const QString strServiceName = replyr.value()[i];
        if (strServiceName.contains("screensaver", Qt::CaseInsensitive))
            serviceNames << strServiceName;
    }
    if (serviceNames.isEmpty())
        LogRel(("QDBus error. No screen saver service found among registered DBus services."));

    return serviceNames;
}

bool NativeWindowSubsystem::X11CheckDBusScreenSaverServices()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!X11CheckDBusConnection(connection))
        return false;

    QDBusReply<QStringList> replyr = connection.interface()->registeredServiceNames();
    if (!replyr.isValid())
    {
        const QDBusError replyError = replyr.error();
        LogRel(("QDBus error. Could not query registered service names %s %s",
                replyError.name().toUtf8().constData(), replyError.message().toUtf8().constData()));
        return false;
    }
    for (int i = 0; i < replyr.value().size(); ++i)
    {
        const QString strServiceName = replyr.value()[i];
        if (strServiceName.contains("screensaver", Qt::CaseInsensitive))
            return true;
    }
    LogRel(("QDBus error. No screen saver service found among registered DBus services."));
    return false;
}

void X11IntrospectInterfaceNode(const QDomElement &interface,
                                const QString &strServiceName,
                                QVector<X11ScreenSaverInhibitMethod*> &methods)
{
    QDomElement child = interface.firstChildElement();
    while (!child.isNull())
    {
        if (child.tagName() == "method" && child.attribute("name") == "Inhibit")
        {
            X11ScreenSaverInhibitMethod *newMethod = new X11ScreenSaverInhibitMethod;
            newMethod->m_iCookie = 0;
            newMethod->m_strServiceName = strServiceName;
            newMethod->m_strInterface = interface.attribute("name");
            newMethod->m_strPath = "/";
            newMethod->m_strPath.append(interface.attribute("name"));
            newMethod->m_strPath.replace(".", "/");
            methods.append(newMethod);
        }
        child = child.nextSiblingElement();
    }
}

void X11IntrospectServices(const QDBusConnection &connection,
                           const QString &strService,
                           const QString &strPath,
                           QVector<X11ScreenSaverInhibitMethod*> &methods)
{
    QDBusMessage call = QDBusMessage::createMethodCall(strService, strPath.isEmpty() ? QLatin1String("/") : strPath,
                                                       QLatin1String("org.freedesktop.DBus.Introspectable"),
                                                       QLatin1String("Introspect"));
    QDBusReply<QString> xmlReply = connection.call(call);

    if (!xmlReply.isValid())
        return;

    QDomDocument doc;
    doc.setContent(xmlReply);
    QDomElement node = doc.documentElement();
    QDomElement child = node.firstChildElement();
    while (!child.isNull())
    {
        if (child.tagName() == QLatin1String("node"))
        {
            QString subPath = strPath + QLatin1Char('/') + child.attribute(QLatin1String("name"));
            X11IntrospectServices(connection, strService, subPath, methods);
        }
        else if (child.tagName() == QLatin1String("interface"))
            X11IntrospectInterfaceNode(child, strService, methods);
        child = child.nextSiblingElement();
    }
}

QVector<X11ScreenSaverInhibitMethod*> NativeWindowSubsystem::X11FindDBusScrenSaverInhibitMethods()
{
    QVector<X11ScreenSaverInhibitMethod*> methods;

    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!X11CheckDBusConnection(connection))
        return methods;

    QStringList services = X11FindDBusScreenSaverServices(connection);
    foreach(const QString &strServiceName, services)
        X11IntrospectServices(connection, strServiceName, "", methods);

    return methods;
}

void NativeWindowSubsystem::X11InhibitUninhibitScrenSaver(bool fInhibit, QVector<X11ScreenSaverInhibitMethod*> &inOutInhibitMethods)
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!X11CheckDBusConnection(connection))
        return;
    for (int i = 0; i < inOutInhibitMethods.size(); ++i)
    {
        QDBusInterface screenSaverInterface(inOutInhibitMethods[i]->m_strServiceName, inOutInhibitMethods[i]->m_strPath,
                                            inOutInhibitMethods[i]->m_strInterface, connection);
        if (!screenSaverInterface.isValid())
        {
            QDBusError error = screenSaverInterface.lastError();
            LogRel(("QDBus error for service %s: %s. %s\n",
                    inOutInhibitMethods[i]->m_strServiceName.toUtf8().constData(),
                    error.name().toUtf8().constData(),
                    error.message().toUtf8().constData()));
            continue;
        }
        QDBusReply<uint> reply;
        if (fInhibit)
        {
            reply = screenSaverInterface.call("Inhibit", "Oracle VirtualBox", "ScreenSaverInhibit");
            if (reply.isValid())
                inOutInhibitMethods[i]->m_iCookie = reply.value();
        }
        else
        {
            reply = screenSaverInterface.call("UnInhibit", inOutInhibitMethods[i]->m_iCookie);
        }
        if (!reply.isValid())
        {
            QDBusError error = reply.error();
            LogRel(("QDBus inhibition call error for service %s: %s. %s\n",
                    inOutInhibitMethods[i]->m_strServiceName.toUtf8().constData(),
                    error.name().toUtf8().constData(),
                    error.message().toUtf8().constData()));
        }
    }
}

char *XXGetProperty(Display *pDpy, Window windowHandle, Atom propType, const char *pszPropName)
{
    Atom propNameAtom = XInternAtom(pDpy, pszPropName, True /* only_if_exists */);
    if (propNameAtom == None)
        return NULL;

    Atom actTypeAtom = None;
    int actFmt = 0;
    unsigned long nItems = 0;
    unsigned long nBytesAfter = 0;
    unsigned char *propVal = NULL;
    int rc = XGetWindowProperty(pDpy, windowHandle, propNameAtom,
                                0, LONG_MAX, False /* delete */,
                                propType, &actTypeAtom, &actFmt,
                                &nItems, &nBytesAfter, &propVal);
    if (rc != Success)
        return NULL;

    return reinterpret_cast<char*>(propVal);
}

bool XXSendClientMessage(Display *pDpy, Window windowHandle, const char *pszMsg,
                         unsigned long aData0 = 0, unsigned long aData1 = 0,
                         unsigned long aData2 = 0, unsigned long aData3 = 0,
                         unsigned long aData4 = 0)
{
    Atom msgAtom = XInternAtom(pDpy, pszMsg, True /* only_if_exists */);
    if (msgAtom == None)
        return false;

    XEvent ev;

    ev.xclient.type = ClientMessage;
    ev.xclient.serial = 0;
    ev.xclient.send_event = True;
    ev.xclient.display = pDpy;
    ev.xclient.window = windowHandle;
    ev.xclient.message_type = msgAtom;

    /* Always send as 32 bit for now: */
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = aData0;
    ev.xclient.data.l[1] = aData1;
    ev.xclient.data.l[2] = aData2;
    ev.xclient.data.l[3] = aData3;
    ev.xclient.data.l[4] = aData4;

    return XSendEvent(pDpy, DefaultRootWindow(pDpy), False,
                      SubstructureRedirectMask, &ev) != 0;
}

bool NativeWindowSubsystem::X11ActivateWindow(WId wId, bool fSwitchDesktop)
{
    bool fResult = true;
    Display *pDisplay = QX11Info::display();

    if (fSwitchDesktop)
    {
        /* Try to find the desktop ID using the NetWM property: */
        CARD32 *pDesktop = (CARD32*)XXGetProperty(pDisplay, wId, XA_CARDINAL, "_NET_WM_DESKTOP");
        if (pDesktop == NULL)
            // WORKAROUND:
            // if the NetWM properly is not supported try to find
            // the desktop ID using the GNOME WM property.
            pDesktop = (CARD32*)XXGetProperty(pDisplay, wId, XA_CARDINAL, "_WIN_WORKSPACE");

        if (pDesktop != NULL)
        {
            bool ok = XXSendClientMessage(pDisplay, DefaultRootWindow(pDisplay), "_NET_CURRENT_DESKTOP", *pDesktop);
            if (!ok)
            {
                Log1WarningFunc(("Couldn't switch to pDesktop=%08X\n", pDesktop));
                fResult = false;
            }
            XFree(pDesktop);
        }
        else
        {
            Log1WarningFunc(("Couldn't find a pDesktop ID for wId=%08X\n", wId));
            fResult = false;
        }
    }

    bool ok = XXSendClientMessage(pDisplay, wId, "_NET_ACTIVE_WINDOW");
    fResult &= !!ok;

    XRaiseWindow(pDisplay, wId);
    return fResult;
}

bool NativeWindowSubsystem::X11SupportsFullScreenMonitorsProtocol()
{
    /* This method tests whether the current X11 window manager supports full-screen mode as we need it.
     * Unfortunately the EWMH specification was not fully clear about whether we can expect to find
     * all of these atoms on the _NET_SUPPORTED root window property, so we have to test with all
     * interesting window managers. If this fails for a user when you think it should succeed
     * they should try executing:
     * xprop -root | egrep -w '_NET_WM_FULLSCREEN_MONITORS|_NET_WM_STATE|_NET_WM_STATE_FULLSCREEN'
     * in an X11 terminal window.
     * All three strings should be found under a property called "_NET_SUPPORTED(ATOM)". */

    /* Using a global to get at the display does not feel right, but that is how it is done elsewhere in the code. */
    Display *pDisplay = QX11Info::display();
    Atom atomSupported            = XInternAtom(pDisplay, "_NET_SUPPORTED",
                                                True /* only_if_exists */);
    Atom atomWMFullScreenMonitors = XInternAtom(pDisplay,
                                                "_NET_WM_FULLSCREEN_MONITORS",
                                                True /* only_if_exists */);
    Atom atomWMState              = XInternAtom(pDisplay,
                                                "_NET_WM_STATE",
                                                True /* only_if_exists */);
    Atom atomWMStateFullScreen    = XInternAtom(pDisplay,
                                                "_NET_WM_STATE_FULLSCREEN",
                                                True /* only_if_exists */);
    bool fFoundFullScreenMonitors = false;
    bool fFoundState              = false;
    bool fFoundStateFullScreen    = false;
    Atom atomType;
    int cFormat;
    unsigned long cItems;
    unsigned long cbLeft;
    Atom *pAtomHints;
    int rc;
    unsigned i;

    if (   atomSupported == None || atomWMFullScreenMonitors == None
        || atomWMState == None || atomWMStateFullScreen == None)
        return false;
    /* Get atom value: */
    rc = XGetWindowProperty(pDisplay, DefaultRootWindow(pDisplay),
                            atomSupported, 0, 0x7fffffff /*LONG_MAX*/,
                            False /* delete */, XA_ATOM, &atomType,
                            &cFormat, &cItems, &cbLeft,
                            (unsigned char **)&pAtomHints);
    if (rc != Success)
        return false;
    if (pAtomHints == NULL)
        return false;
    if (atomType == XA_ATOM && cFormat == 32 && cbLeft == 0)
        for (i = 0; i < cItems; ++i)
        {
            if (pAtomHints[i] == atomWMFullScreenMonitors)
                fFoundFullScreenMonitors = true;
            if (pAtomHints[i] == atomWMState)
                fFoundState = true;
            if (pAtomHints[i] == atomWMStateFullScreen)
                fFoundStateFullScreen = true;
        }
    XFree(pAtomHints);
    return fFoundFullScreenMonitors && fFoundState && fFoundStateFullScreen;
}

bool NativeWindowSubsystem::X11SetFullScreenMonitor(QWidget *pWidget, ulong uScreenId)
{
    return XXSendClientMessage(QX11Info::display(),
                               pWidget->window()->winId(),
                               "_NET_WM_FULLSCREEN_MONITORS",
                               uScreenId, uScreenId, uScreenId, uScreenId,
                               1 /* Source indication (1 = normal application) */);
}

QVector<Atom> flagsNetWmState(QWidget *pWidget)
{
    /* Get display: */
    Display *pDisplay = QX11Info::display();

    /* Prepare atoms: */
    QVector<Atom> resultNetWmState;
    Atom net_wm_state = XInternAtom(pDisplay, "_NET_WM_STATE", True /* only if exists */);

    /* Get the size of the property data: */
    Atom actual_type;
    int iActualFormat;
    ulong uPropertyLength;
    ulong uBytesLeft;
    uchar *pPropertyData = 0;
    if (XGetWindowProperty(pDisplay, pWidget->window()->winId(),
                           net_wm_state, 0, 0, False, XA_ATOM, &actual_type, &iActualFormat,
                           &uPropertyLength, &uBytesLeft, &pPropertyData) == Success &&
        actual_type == XA_ATOM && iActualFormat == 32)
    {
        resultNetWmState.resize(uBytesLeft / 4);
        XFree((char*)pPropertyData);
        pPropertyData = 0;

        /* Fetch all data: */
        if (XGetWindowProperty(pDisplay, pWidget->window()->winId(),
                               net_wm_state, 0, resultNetWmState.size(), False, XA_ATOM, &actual_type, &iActualFormat,
                               &uPropertyLength, &uBytesLeft, &pPropertyData) != Success)
            resultNetWmState.clear();
        else if (uPropertyLength != (ulong)resultNetWmState.size())
            resultNetWmState.resize(uPropertyLength);

        /* Put it into resultNetWmState: */
        if (!resultNetWmState.isEmpty())
            memcpy(resultNetWmState.data(), pPropertyData, resultNetWmState.size() * sizeof(Atom));
        if (pPropertyData)
            XFree((char*)pPropertyData);
    }

    /* Return result: */
    return resultNetWmState;
}

#if 0 // unused for now?
bool NativeWindowSubsystem::isFullScreenFlagSet(QWidget *pWidget)
{
    /* Get display: */
    Display *pDisplay = QX11Info::display();

    /* Prepare atoms: */
    Atom net_wm_state_fullscreen = XInternAtom(pDisplay, "_NET_WM_STATE_FULLSCREEN", True /* only if exists */);

    /* Check if flagsNetWmState(pWidget) contains full-screen flag: */
    return flagsNetWmState(pWidget).contains(net_wm_state_fullscreen);
}

void NativeWindowSubsystem::setFullScreenFlag(QWidget *pWidget)
{
    /* Get display: */
    Display *pDisplay = QX11Info::display();

    /* Prepare atoms: */
    QVector<Atom> resultNetWmState = flagsNetWmState(pWidget);
    Atom net_wm_state = XInternAtom(pDisplay, "_NET_WM_STATE", True /* only if exists */);
    Atom net_wm_state_fullscreen = XInternAtom(pDisplay, "_NET_WM_STATE_FULLSCREEN", True /* only if exists */);

    /* Append resultNetWmState with fullscreen flag if necessary: */
    if (!resultNetWmState.contains(net_wm_state_fullscreen))
    {
        resultNetWmState.append(net_wm_state_fullscreen);
        /* Apply property to widget again: */
        XChangeProperty(pDisplay, pWidget->window()->winId(),
                        net_wm_state, XA_ATOM, 32, PropModeReplace,
                        (unsigned char*)resultNetWmState.data(), resultNetWmState.size());
    }
}
#endif // unused for now?

void NativeWindowSubsystem::X11SetSkipTaskBarFlag(QWidget *pWidget)
{
    /* Get display: */
    Display *pDisplay = QX11Info::display();

    /* Prepare atoms: */
    QVector<Atom> resultNetWmState = flagsNetWmState(pWidget);
    Atom net_wm_state = XInternAtom(pDisplay, "_NET_WM_STATE", True /* only if exists */);
    Atom net_wm_state_skip_taskbar = XInternAtom(pDisplay, "_NET_WM_STATE_SKIP_TASKBAR", True /* only if exists */);

    /* Append resultNetWmState with skip-taskbar flag if necessary: */
    if (!resultNetWmState.contains(net_wm_state_skip_taskbar))
    {
        resultNetWmState.append(net_wm_state_skip_taskbar);
        /* Apply property to widget again: */
        XChangeProperty(pDisplay, pWidget->window()->winId(),
                        net_wm_state, XA_ATOM, 32, PropModeReplace,
                        (unsigned char*)resultNetWmState.data(), resultNetWmState.size());
    }
}

void NativeWindowSubsystem::X11SetSkipPagerFlag(QWidget *pWidget)
{
    /* Get display: */
    Display *pDisplay = QX11Info::display();

    /* Prepare atoms: */
    QVector<Atom> resultNetWmState = flagsNetWmState(pWidget);
    Atom net_wm_state = XInternAtom(pDisplay, "_NET_WM_STATE", True /* only if exists */);
    Atom net_wm_state_skip_pager = XInternAtom(pDisplay, "_NET_WM_STATE_SKIP_PAGER", True /* only if exists */);

    /* Append resultNetWmState with skip-pager flag if necessary: */
    if (!resultNetWmState.contains(net_wm_state_skip_pager))
    {
        resultNetWmState.append(net_wm_state_skip_pager);
        /* Apply property to widget again: */
        XChangeProperty(pDisplay, pWidget->window()->winId(),
                        net_wm_state, XA_ATOM, 32, PropModeReplace,
                        (unsigned char*)resultNetWmState.data(), resultNetWmState.size());
    }
}

void NativeWindowSubsystem::X11SetWMClass(QWidget *pWidget, const QString &strNameString, const QString &strClassString)
{
    /* Make sure all arguments set: */
    AssertReturnVoid(pWidget && !strNameString.isNull() && !strClassString.isNull());

    /* Define QByteArray objects to make sure data is alive within the scope: */
    QByteArray nameByteArray;
    /* Check the existence of RESOURCE_NAME env. variable and override name string if necessary: */
    const char resourceName[] = "RESOURCE_NAME";
    if (qEnvironmentVariableIsSet(resourceName))
        nameByteArray = qgetenv(resourceName);
    else
        nameByteArray = strNameString.toLatin1();
    QByteArray classByteArray = strClassString.toLatin1();

    AssertReturnVoid(nameByteArray.data() && classByteArray.data());

    XClassHint windowClass;
    windowClass.res_name = nameByteArray.data();
    windowClass.res_class = classByteArray.data();
    /* Set WM_CLASS of the window to passed name and class strings: */
    XSetClassHint(QX11Info::display(), pWidget->window()->winId(), &windowClass);
}

void NativeWindowSubsystem::X11SetXwaylandMayGrabKeyboardFlag(QWidget *pWidget)
{
    XXSendClientMessage(QX11Info::display(), pWidget->window()->winId(),
                        "_XWAYLAND_MAY_GRAB_KEYBOARD", 1);
}
