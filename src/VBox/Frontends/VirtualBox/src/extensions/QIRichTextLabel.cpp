/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIRichTextLabel class implementation.
 */

/*
 * Copyright (C) 2012-2022 Oracle Corporation
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
#include <QAccessibleWidget>
#include <QtMath>
#include <QUrl>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"

/* Other VBox includes: */
#include "iprt/assert.h"

/* Forward declarations: */
class QIRichTextLabel;


/** QAccessibleObject extension used as an accessibility interface for QIRichTextLabel. */
class UIAccessibilityInterfaceForQIRichTextLabel : public QAccessibleWidget
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QIRichTextLabel accessibility interface: */
        if (pObject && strClassname == QLatin1String("QIRichTextLabel"))
            return new UIAccessibilityInterfaceForQIRichTextLabel(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    UIAccessibilityInterfaceForQIRichTextLabel(QWidget *pWidget)
        : QAccessibleWidget(pWidget, QAccessible::StaticText)
    {}

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const RT_OVERRIDE;

private:

    /** Returns corresponding QIRichTextLabel. */
    QIRichTextLabel *label() const;
};


/*********************************************************************************************************************************
*   Class UIAccessibilityInterfaceForQIRichTextLabel implementation.                                                             *
*********************************************************************************************************************************/

QString UIAccessibilityInterfaceForQIRichTextLabel::text(QAccessible::Text enmTextRole) const
{
    /* Make sure label still alive: */
    AssertPtrReturn(label(), QString());

    /* Return the description: */
    if (enmTextRole == QAccessible::Description)
        return label()->plainText();

    /* Null-string by default: */
    return QString();
}

QIRichTextLabel *UIAccessibilityInterfaceForQIRichTextLabel::label() const
{
    return qobject_cast<QIRichTextLabel*>(widget());
}


/*********************************************************************************************************************************
*   Class QIRichTextLabel implementation.                                                                                        *
*********************************************************************************************************************************/

QIRichTextLabel::QIRichTextLabel(QWidget *pParent)
    : QWidget(pParent)
    , m_pTextBrowser()
    , m_iMinimumTextWidth(0)
{
    /* Install QIRichTextLabel accessibility interface factory: */
    QAccessible::installFactory(UIAccessibilityInterfaceForQIRichTextLabel::pFactory);

    /* Configure self: */
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Configure layout: */
        pMainLayout->setContentsMargins(0, 0, 0, 0);

        /* Create text-browser: */
        m_pTextBrowser = new QTextBrowser;
        if (m_pTextBrowser)
        {
            /* Configure text-browser: */
            m_pTextBrowser->setReadOnly(true);
            m_pTextBrowser->setFocusPolicy(Qt::NoFocus);
            m_pTextBrowser->setFrameShape(QFrame::NoFrame);
            m_pTextBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_pTextBrowser->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
            m_pTextBrowser->setOpenExternalLinks(true);

            /* Tune text-browser viewport palette: */
            m_pTextBrowser->viewport()->setAutoFillBackground(false);
            QPalette pal = m_pTextBrowser->viewport()->palette();
            pal.setColor(QPalette::Active,   QPalette::Text, pal.color(QPalette::Active,   QPalette::WindowText));
            pal.setColor(QPalette::Inactive, QPalette::Text, pal.color(QPalette::Inactive, QPalette::WindowText));
            pal.setColor(QPalette::Disabled, QPalette::Text, pal.color(QPalette::Disabled, QPalette::WindowText));
            m_pTextBrowser->viewport()->setPalette(pal);

            /* Setup connections finally: */
            connect(m_pTextBrowser, &QTextBrowser::anchorClicked, this, &QIRichTextLabel::sigLinkClicked);
        }

        /* Add into layout: */
        pMainLayout->addWidget(m_pTextBrowser);
    }
}

QString QIRichTextLabel::text() const
{
    return m_pTextBrowser->toHtml();
}

QString QIRichTextLabel::plainText() const
{
    return m_pTextBrowser->toPlainText();
}

void QIRichTextLabel::registerImage(const QImage &image, const QString &strName)
{
    m_pTextBrowser->document()->addResource(QTextDocument::ImageResource, QUrl(strName), QVariant(image));
}

void QIRichTextLabel::registerPixmap(const QPixmap &pixmap, const QString &strName)
{
    m_pTextBrowser->document()->addResource(QTextDocument::ImageResource, QUrl(strName), QVariant(pixmap));
}

QTextOption::WrapMode QIRichTextLabel::wordWrapMode() const
{
    return m_pTextBrowser->wordWrapMode();
}

void QIRichTextLabel::setWordWrapMode(QTextOption::WrapMode policy)
{
    m_pTextBrowser->setWordWrapMode(policy);
}

void QIRichTextLabel::installEventFilter(QObject *pFilterObj)
{
    QWidget::installEventFilter(pFilterObj);
    m_pTextBrowser->installEventFilter(pFilterObj);
}

QFont QIRichTextLabel::browserFont() const
{
    return m_pTextBrowser->font();
}

void QIRichTextLabel::setBrowserFont(const QFont &newFont)
{
    m_pTextBrowser->setFont(newFont);
}

int QIRichTextLabel::minimumTextWidth() const
{
    return m_iMinimumTextWidth;
}

void QIRichTextLabel::setMinimumTextWidth(int iMinimumTextWidth)
{
    /* Remember minimum text width: */
    m_iMinimumTextWidth = iMinimumTextWidth;

    /* Get corresponding QTextDocument: */
    QTextDocument *pTextDocument = m_pTextBrowser->document();
    /* Bug in QTextDocument (?) : setTextWidth doesn't work from the first time. */
    for (int iTry = 0; pTextDocument->textWidth() != m_iMinimumTextWidth && iTry < 3; ++iTry)
        pTextDocument->setTextWidth(m_iMinimumTextWidth);
    /* Get corresponding QTextDocument size: */
    QSize size = pTextDocument->size().toSize();

    /* Resize to content size: */
    m_pTextBrowser->setMinimumSize(size);
    layout()->activate();
}

void QIRichTextLabel::setText(const QString &strText)
{
    /* Set text: */
    m_pTextBrowser->setHtml(strText);

    /* Get corresponding QTextDocument: */
    QTextDocument *pTextDocument = m_pTextBrowser->document();

    // WORKAROUND:
    // Ok, here is the trick.  In Qt 5.6.x initial QTextDocument size is always 0x0
    // even if contents present.  To make QTextDocument calculate initial size we
    // need to pass it some initial text-width, that way size should be calualated
    // on the basis of passed width.  No idea why but in Qt 5.6.x first calculated
    // size doesn't actually linked to initially passed text-width, somehow it
    // always have 640px width and various height which depends on currently set
    // contents.  So, we just using 640px as initial text-width.
    pTextDocument->setTextWidth(640);

    /* Now get that initial size which is 640xY, and propose new text-width as 4/3
     * of hypothetical width current content would have laid out as square: */
    const QSize oldSize = pTextDocument->size().toSize();
    const int iProposedWidth = qSqrt(oldSize.width() * oldSize.height()) * 4 / 3;
    pTextDocument->setTextWidth(iProposedWidth);

    /* Get effective QTextDocument size: */
    const QSize newSize = pTextDocument->size().toSize();

    /* Set minimum text width to corresponding value: */
    setMinimumTextWidth(m_iMinimumTextWidth == 0 ? newSize.width() : m_iMinimumTextWidth);
}
