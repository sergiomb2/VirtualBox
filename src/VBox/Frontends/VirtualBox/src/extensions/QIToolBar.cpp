/* $Id$ */
/** @file
 * VBox Qt GUI - QIToolBar class implementation.
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

/* Qt includes: */
#include <QLayout>
#include <QMainWindow>
#include <QResizeEvent>
#ifdef VBOX_WS_MAC
# include <QApplication>
# include <QPainter>
# include <QPainterPath>
#endif

/* GUI includes: */
#include "QIToolBar.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils.h"
#endif


QIToolBar::QIToolBar(QWidget *pParent /* = 0 */)
    : QToolBar(pParent)
    , m_pMainWindow(qobject_cast<QMainWindow*>(pParent))
#ifdef VBOX_WS_MAC
    , m_fEmulateUnifiedToolbar(false)
    , m_iBrandingWidth(0)
#endif
{
    prepare();
}

void QIToolBar::setUseTextLabels(bool fEnable)
{
    /* Determine tool-button style on the basis of passed flag: */
    Qt::ToolButtonStyle tbs = fEnable ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonIconOnly;

    /* Depending on parent, assign this style: */
    if (m_pMainWindow)
        m_pMainWindow->setToolButtonStyle(tbs);
    else
        setToolButtonStyle(tbs);
}

bool QIToolBar::useTextLabels() const
{
    /* Depending on parent, return the style: */
    if (m_pMainWindow)
        return m_pMainWindow->toolButtonStyle() == Qt::ToolButtonTextUnderIcon;
    else
        return toolButtonStyle() == Qt::ToolButtonTextUnderIcon;
}

#ifdef VBOX_WS_MAC
void QIToolBar::enableMacToolbar()
{
    /* Depending on parent, enable unified title/tool-bar: */
    if (m_pMainWindow)
        m_pMainWindow->setUnifiedTitleAndToolBarOnMac(true);
}

void QIToolBar::emulateMacToolbar()
{
    /* Remember request, to be used in paintEvent: */
    m_fEmulateUnifiedToolbar = true;
}

void QIToolBar::setShowToolBarButton(bool fShow)
{
    ::darwinSetShowsToolbarButton(this, fShow);
}

void QIToolBar::updateLayout()
{
    // WORKAROUND:
    // There is a bug in Qt Cocoa which result in showing a "more arrow" when
    // the necessary size of the tool-bar is increased. Also for some languages
    // the with doesn't match if the text increase. So manually adjust the size
    // after changing the text.
    QSizePolicy sp = sizePolicy();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    adjustSize();
    setSizePolicy(sp);
    layout()->invalidate();
    layout()->activate();
}

void QIToolBar::enableBranding(const QIcon &icnBranding,
                               const QString &strBranding,
                               const QColor &clrBranding,
                               int iBrandingWidth)
{
    m_icnBranding = icnBranding;
    m_strBranding = strBranding;
    m_clrBranding = clrBranding;
    m_iBrandingWidth = iBrandingWidth;
    update();
}
#endif /* VBOX_WS_MAC */

void QIToolBar::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QToolBar::resizeEvent(pEvent);

    /* Notify listeners about new size: */
    emit sigResized(pEvent->size());
}

#ifdef VBOX_WS_MAC
void QIToolBar::paintEvent(QPaintEvent *pEvent)
{
    /* Call to base-class: */
    QToolBar::paintEvent(pEvent);

    /* If we have request to emulate unified tool-bar: */
    if (m_fEmulateUnifiedToolbar)
    {
        /* Limit painting with incoming rectangle: */
        QPainter painter(this);
        painter.setClipRect(pEvent->rect());

        /* Acquire full rectangle: */
        const QRect rectangle = rect();

        /* Prepare gradient: */
        const QColor backgroundColor = QApplication::palette().color(QPalette::Active, QPalette::Window);
        QLinearGradient gradient(rectangle.topLeft(), rectangle.bottomLeft());
        gradient.setColorAt(0, backgroundColor.darker(105));
        gradient.setColorAt(1, backgroundColor.darker(115));

        /* Fill background: */
        painter.fillRect(rectangle, gradient);

        /* Do we have branding stuff? */
        if (!m_icnBranding.isNull())
        {
            /* A bit of common stuff: */
            QFont fnt = font();
            int iTextWidth = 0;
            int iTextHeight = 0;

            /* Configure font to fit width (m_iBrandingWidth - 2 * 4): */
            if (useTextLabels())
            {
                for (int i = 0; i <= 10; ++i) // no more than 10 tries ..
                {
                    if (fnt.pixelSize() == -1)
                        fnt.setPointSize(fnt.pointSize() - i);
                    else
                        fnt.setPixelSize(fnt.pixelSize() - i);
                    iTextWidth = QFontMetrics(fnt).size(0, m_strBranding).width();
                    if (iTextWidth <= m_iBrandingWidth - 2 * 4)
                        break;
                }
                iTextHeight = QFontMetrics(fnt).height();
            }

            /* Draw pixmap: */
            const int iIconSize = qMin(rectangle.height(), 32 /* default */);
            const int iIconMarginH = (m_iBrandingWidth - iIconSize) / 2;
            const int iIconMarginV = (rectangle.height() - iIconSize - iTextHeight) / 2;
            const int iIconX = rectangle.width() - iIconSize - iIconMarginH;
            const int iIconY = iIconMarginV;
            painter.drawPixmap(iIconX, iIconY, m_icnBranding.pixmap(QSize(iIconSize, iIconSize)));

            /* Draw text path: */
            if (useTextLabels())
            {
                const int iTextMargingH = (m_iBrandingWidth - iTextWidth) / 2;
                const int iTextX = rectangle.width() - iTextWidth - iTextMargingH;
                const int iTextY = iIconY + iIconSize + iTextHeight;
                QPainterPath textPath;
                textPath.addText(0, 0, fnt, m_strBranding);
                textPath.translate(iTextX, iTextY);
                painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
                painter.setPen(QPen(m_clrBranding.darker(80), 2, Qt::SolidLine, Qt::RoundCap));
                painter.drawPath(QPainterPathStroker().createStroke(textPath));
                painter.setBrush(Qt::black);
                painter.setPen(Qt::NoPen);
                painter.drawPath(textPath);
            }
        }
    }
}
#endif /* VBOX_WS_MAC */

void QIToolBar::prepare()
{
    /* Configure tool-bar: */
    setFloatable(false);
    setMovable(false);

#ifdef VBOX_WS_MAC
    setStyleSheet("QToolBar { border: 0px none black; }");
#endif

    /* Configure tool-bar' layout: */
    if (layout())
        layout()->setContentsMargins(0, 0, 0, 0);

    /* Configure tool-bar' context-menu policy: */
    setContextMenuPolicy(Qt::NoContextMenu);
}
