/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2022 Oracle Corporation
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
#include <QHBoxLayout>
#include <QMenu>
#include <QSpinBox>
#include <QTextEdit>
#include <QTime>

/* GUI includes: */
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIFileManager.h"
#include "UIFileManagerLogPanel.h"


/*********************************************************************************************************************************
*   UIFileManagerLogViewer definition.                                                                                   *
*********************************************************************************************************************************/

class UIFileManagerLogViewer : public QTextEdit
{

    Q_OBJECT;

public:

    UIFileManagerLogViewer(QWidget *pParent = 0);

protected:

    virtual void contextMenuEvent(QContextMenuEvent * event) RT_OVERRIDE;

private slots:

    void sltClear();
};

/*********************************************************************************************************************************
*   UIFileManagerLogViewer implementation.                                                                                   *
*********************************************************************************************************************************/

UIFileManagerLogViewer::UIFileManagerLogViewer(QWidget *pParent /* = 0 */)
    :QTextEdit(pParent)
{
    setUndoRedoEnabled(false);
    setReadOnly(true);
}

void UIFileManagerLogViewer::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();

    QAction *pClearAction = menu->addAction(UIFileManager::tr("Clear"));
    connect(pClearAction, &QAction::triggered, this, &UIFileManagerLogViewer::sltClear);
    menu->exec(event->globalPos());
    delete menu;
}

void UIFileManagerLogViewer::sltClear()
{
    clear();
}


/*********************************************************************************************************************************
*   UIFileManagerLogPanel implementation.                                                                            *
*********************************************************************************************************************************/

UIFileManagerLogPanel::UIFileManagerLogPanel(QWidget *pParent /* = 0 */)
    : UIDialogPanel(pParent)
    , m_pLogTextEdit(0)
{
    prepare();
}

void UIFileManagerLogPanel::appendLog(const QString &strLog, const QString &strMachineName, FileManagerLogType eLogType)
{
    if (!m_pLogTextEdit)
        return;
    QString strStartTag("<font color=\"Black\">");
    QString strEndTag("</font>");
    if (eLogType == FileManagerLogType_Error)
    {
        strStartTag = "<b><font color=\"Red\">";
        strEndTag = "</font></b>";
    }
    QString strColoredLog = QString("%1 %2: %3 %4 %5").arg(strStartTag).arg(QTime::currentTime().toString("hh:mm:ss:z")).arg(strMachineName).arg(strLog).arg(strEndTag);
    m_pLogTextEdit->append(strColoredLog);
}

QString UIFileManagerLogPanel::panelName() const
{
    return "LogPanel";
}

void UIFileManagerLogPanel::prepareWidgets()
{
    if (!mainLayout())
        return;
    m_pLogTextEdit = new UIFileManagerLogViewer;
    if (m_pLogTextEdit)
    {
        mainLayout()->addWidget(m_pLogTextEdit);
    }
}

void UIFileManagerLogPanel::prepareConnections()
{
}

void UIFileManagerLogPanel::retranslateUi()
{
    UIDialogPanel::retranslateUi();

}


#include "UIFileManagerLogPanel.moc"
