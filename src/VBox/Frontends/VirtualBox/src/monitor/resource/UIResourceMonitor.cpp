/* $Id$ */
/** @file
 * VBox Qt GUI - UIResourceMonitor class implementation.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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
#include <QAbstractTableModel>
#include <QCheckBox>
#include <QHeaderView>
#include <QItemDelegate>
#include <QLabel>
#include <QMenuBar>
#include <QPainter>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QSortFilterProxyModel>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIActionPoolManager.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIExtraDataDefs.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIPerformanceMonitor.h"
#include "UIResourceMonitor.h"
#include "UIMessageCenter.h"
#include "UIToolBar.h"
#include "UIVirtualBoxEventHandler.h"

#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */

/* COM includes: */
#include "CConsole.h"
#include "CMachine.h"
#include "CMachineDebugger.h"
#include "CPerformanceMetric.h"

/* Other VBox includes: */
#include <iprt/cidr.h>

struct ResourceColumn
{
    QString m_strName;
    bool    m_fEnabled;
};

/** Draws a doughnut shaped chart for the passed data values and can have a text drawn in the center. */


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorDoughnutChart definition.                                                                           *
*********************************************************************************************************************************/

class UIVMResourceMonitorDoughnutChart : public QWidget
{

    Q_OBJECT;

public:

    UIVMResourceMonitorDoughnutChart(QWidget *pParent = 0);
    void updateData(quint64 iData0, quint64 iData1);
    void setChartColors(const QColor &color0, const QColor &color1);
    void setChartCenterString(const QString &strCenter);
    void setDataMaximum(quint64 iMax);

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

private:

    quint64 m_iData0;
    quint64 m_iData1;
    quint64 m_iDataMaximum;
    int m_iMargin;
    QColor m_color0;
    QColor m_color1;
    /** If not empty this text is drawn at the center of the doughnut chart. */
    QString m_strCenter;
};

/** A simple container to store host related performance values. */


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorHostStats definition.                                                                               *
*********************************************************************************************************************************/

class UIVMResourceMonitorHostStats
{

public:

    UIVMResourceMonitorHostStats();
    quint64 m_iCPUUserLoad;
    quint64 m_iCPUKernelLoad;
    quint64 m_iCPUFreq;
    quint64 m_iRAMTotal;
    quint64 m_iRAMFree;
    quint64 m_iFSTotal;
    quint64 m_iFSFree;
};

/** A container QWidget to layout host stats. related widgets. */


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorHostStatsWidget definition.                                                                         *
*********************************************************************************************************************************/

class UIVMResourceMonitorHostStatsWidget : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

public:

    UIVMResourceMonitorHostStatsWidget(QWidget *pParent = 0);
    void setHostStats(const UIVMResourceMonitorHostStats &hostStats);

protected:

    virtual void retranslateUi() /* override */;

private:

    void prepare();
    void addVerticalLine(QHBoxLayout *pLayout);
    void updateLabels();

    UIVMResourceMonitorDoughnutChart   *m_pHostCPUChart;
    UIVMResourceMonitorDoughnutChart   *m_pHostRAMChart;
    UIVMResourceMonitorDoughnutChart   *m_pHostFSChart;
    QLabel                             *m_pCPUTitleLabel;
    QLabel                             *m_pCPUUserLabel;
    QLabel                             *m_pCPUKernelLabel;
    QLabel                             *m_pCPUTotalLabel;
    QLabel                             *m_pRAMTitleLabel;
    QLabel                             *m_pRAMUsedLabel;
    QLabel                             *m_pRAMFreeLabel;
    QLabel                             *m_pRAMTotalLabel;
    QLabel                             *m_pFSTitleLabel;
    QLabel                             *m_pFSUsedLabel;
    QLabel                             *m_pFSFreeLabel;
    QLabel                             *m_pFSTotalLabel;
    QColor                              m_CPUUserColor;
    QColor                              m_CPUKernelColor;
    QColor                              m_RAMFreeColor;
    QColor                              m_RAMUsedColor;
    UIVMResourceMonitorHostStats        m_hostStats;
};


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorTableView definition.                                                                               *
*********************************************************************************************************************************/
/** A QTableView extension so manage the column width a bit better than what Qt offers out of box. */
class UIVMResourceMonitorTableView : public QTableView
{
    Q_OBJECT;

signals:

    void sigSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

public:

    UIVMResourceMonitorTableView(QWidget *pParent = 0);
    void setMinimumColumnWidths(const QMap<int, int>& widths);
    void updateColumVisibility();
    int selectedItemIndex() const;
    bool hasSelection() const;

protected:

    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
    virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) /* override */;
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;

private slots:



private:

    /** Resizes all the columns in response to resizeEvent. Columns cannot be narrower than m_minimumColumnWidths values. */
    void resizeHeaders();
    /** Value is in pixels. Columns cannot be narrower than this width. */
    QMap<int, int> m_minimumColumnWidths;
};

/** Each instance of UIVMResourceMonitorItem corresponds to a running vm whose stats are displayed.
  * they are owned my the model. */
/*********************************************************************************************************************************
 *   Class UIVMResourceMonitorItem definition.                                                                           *
 *********************************************************************************************************************************/
class UIResourceMonitorItem
{
public:
    UIResourceMonitorItem(const QUuid &uid, const QString &strVMName);
    UIResourceMonitorItem(const QUuid &uid);
    UIResourceMonitorItem();
    UIResourceMonitorItem(const UIResourceMonitorItem &item);
    ~UIResourceMonitorItem();
    bool operator==(const UIResourceMonitorItem& other) const;
    bool isWithGuestAdditions();

    QUuid    m_VMuid;
    QString  m_strVMName;
    quint64  m_uCPUGuestLoad;
    quint64  m_uCPUVMMLoad;

    quint64  m_uTotalRAM;
    quint64  m_uFreeRAM;
    quint64  m_uUsedRAM;
    float    m_fRAMUsagePercentage;

    quint64 m_uNetworkDownRate;
    quint64 m_uNetworkUpRate;
    quint64 m_uNetworkDownTotal;
    quint64 m_uNetworkUpTotal;

    quint64 m_uDiskWriteRate;
    quint64 m_uDiskReadRate;
    quint64 m_uDiskWriteTotal;
    quint64 m_uDiskReadTotal;

    quint64 m_uVMExitRate;
    quint64 m_uVMExitTotal;

    CSession m_comSession;
    CMachineDebugger m_comDebugger;
    CGuest   m_comGuest;
    /** The strings of each column for the item. We update this during performance query
      * instead of model's data function to know the string length earlier. */
    QMap<int, QString> m_columnData;

private:

    void setupPerformanceCollector();
};

Q_DECLARE_METATYPE(UIResourceMonitorItem);


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorProxyModel definition.                                                                              *
*********************************************************************************************************************************/
class UIResourceMonitorProxyModel : public QSortFilterProxyModel
{

    Q_OBJECT;

public:

    UIResourceMonitorProxyModel(QObject *parent = 0);
    void dataUpdate();
};


/*********************************************************************************************************************************
*   Class UIResourceMonitorModel definition.                                                                                     *
*********************************************************************************************************************************/
class UIResourceMonitorModel : public QAbstractTableModel
{
    Q_OBJECT;

signals:

    void sigDataUpdate();
    void sigHostStatsUpdate(const UIVMResourceMonitorHostStats &stats);

public:

    UIResourceMonitorModel(QObject *parent = 0);
    int      rowCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    int      columnCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    QVariant data(const QModelIndex &index, int role) const /* override */;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    void setColumnCaptions(const QMap<int, QString>& captions);
    void setColumnVisible(const QMap<int, bool>& columnVisible);
    bool columnVisible(int iColumnId) const;
    void setShouldUpdate(bool fShouldUpdate);
    const QMap<int, int> dataLengths() const;
    QUuid itemUid(int iIndex);
    int itemIndex(const QUuid &uid);

private slots:

    void sltMachineStateChanged(const QUuid &uId, const KMachineState state);
    void sltTimeout();

private:

    void initialize();
    void initializeItems();
    void setupPerformanceCollector();
    void queryPerformanceCollector();
    void addItem(const QUuid& uMachineId, const QString& strMachineName);
    void removeItem(const QUuid& uMachineId);
    void getHostRAMStats();

    QVector<UIResourceMonitorItem> m_itemList;
    QMap<int, QString> m_columnTitles;
    QTimer *m_pTimer;
    /** @name The following are used during UIPerformanceCollector::QueryMetricsData(..)
     * @{ */
    QVector<QString> m_nameList;
    QVector<CUnknown> m_objectList;
    /** @} */
    CPerformanceCollector m_performanceMonitor;
    QMap<int, bool> m_columnVisible;
    /** If true the table data and corresponding view is updated. Possibly set by host widget to true only
     *  when the widget is visible in the main UI. */
    bool m_fShouldUpdate;
    UIVMResourceMonitorHostStats m_hostStats;
    /** Maximum length of string length of data displayed in column. Updated in UIResourceMonitorModel::data(..). */
    mutable QMap<int, int> m_columnDataMaxLength;
};


/*********************************************************************************************************************************
*   UIVMResourceMonitorDelegate definition.                                                                                      *
*********************************************************************************************************************************/
/** A QItemDelegate child class to disable dashed lines drawn around selected cells in QTableViews */
class UIVMResourceMonitorDelegate : public QItemDelegate
{

    Q_OBJECT;

protected:

    virtual void drawFocus ( QPainter * /*painter*/, const QStyleOptionViewItem & /*option*/, const QRect & /*rect*/ ) const {}
};


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorDoughnutChart implementation.                                                                       *
*********************************************************************************************************************************/
UIVMResourceMonitorDoughnutChart::UIVMResourceMonitorDoughnutChart(QWidget *pParent /* = 0 */)
    :QWidget(pParent)
    , m_iData0(0)
    , m_iData1(0)
    , m_iDataMaximum(0)
    , m_iMargin(3)
{
}

void UIVMResourceMonitorDoughnutChart::updateData(quint64 iData0, quint64 iData1)
{
    m_iData0 = iData0;
    m_iData1 = iData1;
    update();
}

void UIVMResourceMonitorDoughnutChart::setChartColors(const QColor &color0, const QColor &color1)
{
    m_color0 = color0;
    m_color1 = color1;
}

void UIVMResourceMonitorDoughnutChart::setChartCenterString(const QString &strCenter)
{
    m_strCenter = strCenter;
}

void UIVMResourceMonitorDoughnutChart::setDataMaximum(quint64 iMax)
{
    m_iDataMaximum = iMax;
}

void UIVMResourceMonitorDoughnutChart::paintEvent(QPaintEvent *pEvent)
{
    QWidget::paintEvent(pEvent);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int iFrameHeight = height()- 2 * m_iMargin;
    QRectF outerRect = QRectF(QPoint(m_iMargin,m_iMargin), QSize(iFrameHeight, iFrameHeight));
    QRectF innerRect = UIMonitorCommon::getScaledRect(outerRect, 0.6f, 0.6f);
    UIMonitorCommon::drawCombinedDoughnutChart(m_iData0, m_color0,
                                               m_iData1, m_color1,
                                               painter, m_iDataMaximum,
                                               outerRect, innerRect, 80);
    if (!m_strCenter.isEmpty())
    {
        float mul = 1.f / 1.4f;
        QRectF textRect =  UIMonitorCommon::getScaledRect(innerRect, mul, mul);
        painter.setPen(Qt::black);
        painter.drawText(textRect, Qt::AlignCenter, m_strCenter);
    }
}


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorHostStatsWidget implementation.                                                                     *
*********************************************************************************************************************************/

UIVMResourceMonitorHostStatsWidget::UIVMResourceMonitorHostStatsWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pHostCPUChart(0)
    , m_pHostRAMChart(0)
    , m_pHostFSChart(0)
    , m_pCPUTitleLabel(0)
    , m_pCPUUserLabel(0)
    , m_pCPUKernelLabel(0)
    , m_pCPUTotalLabel(0)
    , m_pRAMTitleLabel(0)
    , m_pRAMUsedLabel(0)
    , m_pRAMFreeLabel(0)
    , m_pRAMTotalLabel(0)
    , m_pFSTitleLabel(0)
    , m_pFSUsedLabel(0)
    , m_pFSFreeLabel(0)
    , m_pFSTotalLabel(0)
    , m_CPUUserColor(Qt::red)
    , m_CPUKernelColor(Qt::blue)
    , m_RAMFreeColor(Qt::blue)
    , m_RAMUsedColor(Qt::red)
{
    prepare();
    retranslateUi();
}

void UIVMResourceMonitorHostStatsWidget::setHostStats(const UIVMResourceMonitorHostStats &hostStats)
{
    m_hostStats = hostStats;
    if (m_pHostCPUChart)
    {
        m_pHostCPUChart->updateData(m_hostStats.m_iCPUUserLoad, m_hostStats.m_iCPUKernelLoad);
        QString strCenter = QString("%1\nMHz").arg(m_hostStats.m_iCPUFreq);
        m_pHostCPUChart->setChartCenterString(strCenter);
    }
    if (m_pHostRAMChart)
    {
        quint64 iUsedRAM = m_hostStats.m_iRAMTotal - m_hostStats.m_iRAMFree;
        m_pHostRAMChart->updateData(iUsedRAM, m_hostStats.m_iRAMFree);
        m_pHostRAMChart->setDataMaximum(m_hostStats.m_iRAMTotal);
        if (m_hostStats.m_iRAMTotal != 0)
        {
            quint64 iUsedRamPer = 100 * (iUsedRAM / (float) m_hostStats.m_iRAMTotal);
            QString strCenter = QString("%1%\n%2").arg(iUsedRamPer).arg(UIResourceMonitorWidget::tr("Used"));
            m_pHostRAMChart->setChartCenterString(strCenter);
        }
    }
    if (m_pHostFSChart)
    {
        quint64 iUsedFS = m_hostStats.m_iFSTotal - m_hostStats.m_iFSFree;
        m_pHostFSChart->updateData(iUsedFS, m_hostStats.m_iFSFree);
        m_pHostFSChart->setDataMaximum(m_hostStats.m_iFSTotal);
        if (m_hostStats.m_iFSTotal != 0)
        {
            quint64 iUsedRamPer = 100 * (iUsedFS / (float) m_hostStats.m_iFSTotal);
            QString strCenter = QString("%1%\n%2").arg(iUsedRamPer).arg(UIResourceMonitorWidget::tr("Used"));
            m_pHostFSChart->setChartCenterString(strCenter);
        }
    }

    updateLabels();
}

void UIVMResourceMonitorHostStatsWidget::retranslateUi()
{
    updateLabels();
}

void UIVMResourceMonitorHostStatsWidget::addVerticalLine(QHBoxLayout *pLayout)
{
    QFrame *pLine = new QFrame;
    pLine->setFrameShape(QFrame::VLine);
    pLine->setFrameShadow(QFrame::Sunken);
    pLayout->addWidget(pLine);
}

void UIVMResourceMonitorHostStatsWidget::prepare()
{
    QHBoxLayout *pLayout = new QHBoxLayout;
    setLayout(pLayout);
    int iMinimumSize =  3 * QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);

    /* CPU Stuff: */
    {
        /* Host CPU Labels: */
        QWidget *pCPULabelContainer = new QWidget;
        pCPULabelContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        pLayout->addWidget(pCPULabelContainer);
        QVBoxLayout *pCPULabelsLayout = new QVBoxLayout;
        pCPULabelsLayout->setContentsMargins(0, 0, 0, 0);
        pCPULabelContainer->setLayout(pCPULabelsLayout);
        m_pCPUTitleLabel = new QLabel;
        pCPULabelsLayout->addWidget(m_pCPUTitleLabel);
        m_pCPUUserLabel = new QLabel;
        pCPULabelsLayout->addWidget(m_pCPUUserLabel);
        m_pCPUKernelLabel = new QLabel;
        pCPULabelsLayout->addWidget(m_pCPUKernelLabel);
        m_pCPUTotalLabel = new QLabel;
        pCPULabelsLayout->addWidget(m_pCPUTotalLabel);
        pCPULabelsLayout->setAlignment(Qt::AlignTop);
        pCPULabelsLayout->setSpacing(0);
        /* Host CPU chart widget: */
        m_pHostCPUChart = new UIVMResourceMonitorDoughnutChart;
        if (m_pHostCPUChart)
        {
            m_pHostCPUChart->setMinimumSize(iMinimumSize, iMinimumSize);
            m_pHostCPUChart->setDataMaximum(100);
            pLayout->addWidget(m_pHostCPUChart);
            m_pHostCPUChart->setChartColors(m_CPUUserColor, m_CPUKernelColor);
        }
    }
    addVerticalLine(pLayout);
    /* RAM Stuff: */
    {
        QWidget *pRAMLabelContainer = new QWidget;
        pRAMLabelContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

        pLayout->addWidget(pRAMLabelContainer);
        QVBoxLayout *pRAMLabelsLayout = new QVBoxLayout;
        pRAMLabelsLayout->setContentsMargins(0, 0, 0, 0);
        pRAMLabelsLayout->setSpacing(0);
        pRAMLabelContainer->setLayout(pRAMLabelsLayout);
        m_pRAMTitleLabel = new QLabel;
        pRAMLabelsLayout->addWidget(m_pRAMTitleLabel);
        m_pRAMUsedLabel = new QLabel;
        pRAMLabelsLayout->addWidget(m_pRAMUsedLabel);
        m_pRAMFreeLabel = new QLabel;
        pRAMLabelsLayout->addWidget(m_pRAMFreeLabel);
        m_pRAMTotalLabel = new QLabel;
        pRAMLabelsLayout->addWidget(m_pRAMTotalLabel);

        m_pHostRAMChart = new UIVMResourceMonitorDoughnutChart;
        if (m_pHostRAMChart)
        {
            m_pHostRAMChart->setMinimumSize(iMinimumSize, iMinimumSize);
            pLayout->addWidget(m_pHostRAMChart);
            m_pHostRAMChart->setChartColors(m_RAMUsedColor, m_RAMFreeColor);
        }
    }
    addVerticalLine(pLayout);
    /* FS Stuff: */
    {
        QWidget *pFSLabelContainer = new QWidget;
        pLayout->addWidget(pFSLabelContainer);
        pFSLabelContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        QVBoxLayout *pFSLabelsLayout = new QVBoxLayout;
        pFSLabelsLayout->setContentsMargins(0, 0, 0, 0);
        pFSLabelsLayout->setSpacing(0);
        pFSLabelContainer->setLayout(pFSLabelsLayout);
        m_pFSTitleLabel = new QLabel;
        pFSLabelsLayout->addWidget(m_pFSTitleLabel);
        m_pFSUsedLabel = new QLabel;
        pFSLabelsLayout->addWidget(m_pFSUsedLabel);
        m_pFSFreeLabel = new QLabel;
        pFSLabelsLayout->addWidget(m_pFSFreeLabel);
        m_pFSTotalLabel = new QLabel;
        pFSLabelsLayout->addWidget(m_pFSTotalLabel);

        m_pHostFSChart = new UIVMResourceMonitorDoughnutChart;
        if (m_pHostFSChart)
        {
            m_pHostFSChart->setMinimumSize(iMinimumSize, iMinimumSize);
            pLayout->addWidget(m_pHostFSChart);
            m_pHostFSChart->setChartColors(m_RAMUsedColor, m_RAMFreeColor);
        }

    }
    pLayout->addStretch(2);
}

void UIVMResourceMonitorHostStatsWidget::updateLabels()
{
    if (m_pCPUTitleLabel)
        m_pCPUTitleLabel->setText(QString("<b>%1</b>").arg(UIResourceMonitorWidget::tr("Host CPU Load")));
    if (m_pCPUUserLabel)
    {
        QString strColor = QColor(m_CPUUserColor).name(QColor::HexRgb);
        m_pCPUUserLabel->setText(QString("<font color=\"%1\">%2: %3%</font>").arg(strColor).arg(UIResourceMonitorWidget::tr("User")).arg(QString::number(m_hostStats.m_iCPUUserLoad)));
    }
    if (m_pCPUKernelLabel)
    {
        QString strColor = QColor(m_CPUKernelColor).name(QColor::HexRgb);
        m_pCPUKernelLabel->setText(QString("<font color=\"%1\">%2: %3%</font>").arg(strColor).arg(UIResourceMonitorWidget::tr("Kernel")).arg(QString::number(m_hostStats.m_iCPUKernelLoad)));
    }
    if (m_pCPUTotalLabel)
        m_pCPUTotalLabel->setText(QString("%1: %2%").arg(UIResourceMonitorWidget::tr("Total")).arg(m_hostStats.m_iCPUUserLoad + m_hostStats.m_iCPUKernelLoad));
    if (m_pRAMTitleLabel)
        m_pRAMTitleLabel->setText(QString("<b>%1</b>").arg(UIResourceMonitorWidget::tr("Host RAM Usage")));
    if (m_pRAMFreeLabel)
    {
        QString strRAM = uiCommon().formatSize(m_hostStats.m_iRAMFree);
        QString strColor = QColor(m_RAMFreeColor).name(QColor::HexRgb);
        m_pRAMFreeLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(UIResourceMonitorWidget::tr("Free")).arg(strRAM));
    }
    if (m_pRAMUsedLabel)
    {
        QString strRAM = uiCommon().formatSize(m_hostStats.m_iRAMTotal - m_hostStats.m_iRAMFree);
        QString strColor = QColor(m_RAMUsedColor).name(QColor::HexRgb);
        m_pRAMUsedLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(UIResourceMonitorWidget::tr("Used")).arg(strRAM));
    }
    if (m_pRAMTotalLabel)
    {
        QString strRAM = uiCommon().formatSize(m_hostStats.m_iRAMTotal);
        m_pRAMTotalLabel->setText(QString("%1: %2").arg(UIResourceMonitorWidget::tr("Total")).arg(strRAM));
    }
    if (m_pFSTitleLabel)
        m_pFSTitleLabel->setText(QString("<b>%1</b>").arg(UIResourceMonitorWidget::tr("Host File System")));
    if (m_pFSFreeLabel)
    {
        QString strFS = uiCommon().formatSize(m_hostStats.m_iFSFree);
        QString strColor = QColor(m_RAMFreeColor).name(QColor::HexRgb);
        m_pFSFreeLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(UIResourceMonitorWidget::tr("Free")).arg(strFS));
    }
    if (m_pFSUsedLabel)
    {
        QString strFS = uiCommon().formatSize(m_hostStats.m_iFSTotal - m_hostStats.m_iFSFree);
        QString strColor = QColor(m_RAMUsedColor).name(QColor::HexRgb);
        m_pFSUsedLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(UIResourceMonitorWidget::tr("Used")).arg(strFS));
    }
    if (m_pFSTotalLabel)
    {
        QString strFS = uiCommon().formatSize(m_hostStats.m_iFSTotal);
        m_pFSTotalLabel->setText(QString("%1: %2").arg(UIResourceMonitorWidget::tr("Total")).arg(strFS));
    }
}



/*********************************************************************************************************************************
*   Class UIVMResourceMonitorTableView implementation.                                                                           *
*********************************************************************************************************************************/

UIVMResourceMonitorTableView::UIVMResourceMonitorTableView(QWidget *pParent /* = 0 */)
    :QTableView(pParent)
{
}

void UIVMResourceMonitorTableView::setMinimumColumnWidths(const QMap<int, int>& widths)
{
    m_minimumColumnWidths = widths;
    resizeHeaders();
}

void UIVMResourceMonitorTableView::updateColumVisibility()
{
    UIResourceMonitorProxyModel *pProxyModel = qobject_cast<UIResourceMonitorProxyModel *>(model());
    if (!pProxyModel)
        return;
    UIResourceMonitorModel *pModel = qobject_cast<UIResourceMonitorModel *>(pProxyModel->sourceModel());
    QHeaderView *pHeader = horizontalHeader();

    if (!pModel || !pHeader)
        return;
    for (int i = (int)VMResourceMonitorColumn_Name; i < (int)VMResourceMonitorColumn_Max; ++i)
    {
        if (!pModel->columnVisible(i))
            pHeader->hideSection(i);
        else
            pHeader->showSection(i);
    }
    resizeHeaders();
}

int UIVMResourceMonitorTableView::selectedItemIndex() const
{
    UIResourceMonitorProxyModel *pModel = qobject_cast<UIResourceMonitorProxyModel*>(model());
    if (!pModel)
        return -1;

    QItemSelectionModel *pSelectionModel =  selectionModel();
    if (!pSelectionModel)
        return -1;
    QModelIndexList selectedItemIndices = pSelectionModel->selectedRows();
    if (selectedItemIndices.isEmpty())
        return -1;

    /* just use the the 1st index: */
    QModelIndex modelIndex = pModel->mapToSource(selectedItemIndices[0]);

    if (!modelIndex.isValid())
        return -1;
    return modelIndex.row();
}

bool UIVMResourceMonitorTableView::hasSelection() const
{
    if (!selectionModel())
        return false;
    return selectionModel()->hasSelection();
}

void UIVMResourceMonitorTableView::resizeEvent(QResizeEvent *pEvent)
{
    resizeHeaders();
    QTableView::resizeEvent(pEvent);
}

void UIVMResourceMonitorTableView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    emit sigSelectionChanged(selected, deselected);
    QTableView::selectionChanged(selected, deselected);
}

void UIVMResourceMonitorTableView::mousePressEvent(QMouseEvent *pEvent)
{
    if (!indexAt(pEvent->pos()).isValid())
        clearSelection();
    QTableView::mousePressEvent(pEvent);
}

void UIVMResourceMonitorTableView::resizeHeaders()
{
    QHeaderView* pHeader = horizontalHeader();
    if (!pHeader)
        return;
    int iSectionCount = pHeader->count();
    int iHiddenSectionCount = pHeader->hiddenSectionCount();
    int iWidth = width() / (iSectionCount - iHiddenSectionCount);
    for (int i = 0; i < iSectionCount; ++i)
    {
        if (pHeader->isSectionHidden(i))
            continue;
        int iMinWidth = m_minimumColumnWidths.value((VMResourceMonitorColumn)i, 0);
        pHeader->resizeSection(i, iWidth < iMinWidth ? iMinWidth : iWidth);
    }
}


/*********************************************************************************************************************************
 *   Class UIVMResourceMonitorItem implementation.                                                                           *
 *********************************************************************************************************************************/
UIResourceMonitorItem::UIResourceMonitorItem(const QUuid &uid, const QString &strVMName)
    : m_VMuid(uid)
    , m_strVMName(strVMName)
    , m_uCPUGuestLoad(0)
    , m_uCPUVMMLoad(0)
    , m_uTotalRAM(0)
    , m_uFreeRAM(0)
    , m_uUsedRAM(0)
    , m_fRAMUsagePercentage(0)
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
    , m_uVMExitRate(0)
    , m_uVMExitTotal(0)
{
    m_comSession = uiCommon().openSession(uid, KLockType_Shared);
    if (!m_comSession.isNull())
    {
        CConsole comConsole = m_comSession.GetConsole();
        if (!comConsole.isNull())
        {
            m_comGuest = comConsole.GetGuest();
            m_comDebugger = comConsole.GetDebugger();
        }
    }
}

UIResourceMonitorItem::UIResourceMonitorItem()
    : m_VMuid(QUuid())
    , m_uCPUGuestLoad(0)
    , m_uCPUVMMLoad(0)
    , m_uTotalRAM(0)
    , m_uUsedRAM(0)
    , m_fRAMUsagePercentage(0)
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
    , m_uVMExitRate(0)
    , m_uVMExitTotal(0)
{
}

UIResourceMonitorItem::UIResourceMonitorItem(const QUuid &uid)
    : m_VMuid(uid)
    , m_uCPUGuestLoad(0)
    , m_uCPUVMMLoad(0)
    , m_uTotalRAM(0)
    , m_uUsedRAM(0)
    , m_fRAMUsagePercentage(0)
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
    , m_uVMExitRate(0)
    , m_uVMExitTotal(0)
{
}

UIResourceMonitorItem::~UIResourceMonitorItem()
{
    if (!m_comSession.isNull())
        m_comSession.UnlockMachine();
}

bool UIResourceMonitorItem::operator==(const UIResourceMonitorItem& other) const
{
    if (m_VMuid == other.m_VMuid)
        return true;
    return false;
}

bool UIResourceMonitorItem::isWithGuestAdditions()
{
    if (m_comGuest.isNull())
        return false;
    return m_comGuest.GetAdditionsStatus(m_comGuest.GetAdditionsRunLevel());
}


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorHostStats implementation.                                                                           *
*********************************************************************************************************************************/

UIVMResourceMonitorHostStats::UIVMResourceMonitorHostStats()
    : m_iCPUUserLoad(0)
    , m_iCPUKernelLoad(0)
    , m_iCPUFreq(0)
    , m_iRAMTotal(0)
    , m_iRAMFree(0)
    , m_iFSTotal(0)
    , m_iFSFree(0)
{
}


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorProxyModel implementation.                                                                          *
*********************************************************************************************************************************/
UIResourceMonitorProxyModel::UIResourceMonitorProxyModel(QObject *parent /* = 0 */)
    :QSortFilterProxyModel(parent)
{
}

void UIResourceMonitorProxyModel::dataUpdate()
{
    if (sourceModel())
        emit dataChanged(index(0,0), index(sourceModel()->rowCount(), sourceModel()->columnCount()));
    invalidate();
}


/*********************************************************************************************************************************
*   Class UIResourceMonitorModel implementation.                                                                                 *
*********************************************************************************************************************************/
UIResourceMonitorModel::UIResourceMonitorModel(QObject *parent /*= 0*/)
    :QAbstractTableModel(parent)
    , m_pTimer(new QTimer(this))
    , m_fShouldUpdate(true)
{
    initialize();
}

void UIResourceMonitorModel::initialize()
{
    for (int i = 0; i < (int)VMResourceMonitorColumn_Max; ++i)
        m_columnDataMaxLength[i] = 0;

    initializeItems();
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIResourceMonitorModel::sltMachineStateChanged);

    if (m_pTimer)
    {
        connect(m_pTimer, &QTimer::timeout, this, &UIResourceMonitorModel::sltTimeout);
        m_pTimer->start(1000);
    }
}

int UIResourceMonitorModel::rowCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return m_itemList.size();
}

int UIResourceMonitorModel::columnCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return VMResourceMonitorColumn_Max;
}

void UIResourceMonitorModel::setShouldUpdate(bool fShouldUpdate)
{
    m_fShouldUpdate = fShouldUpdate;
}

const QMap<int, int> UIResourceMonitorModel::dataLengths() const
{
    return m_columnDataMaxLength;
}

QUuid UIResourceMonitorModel::itemUid(int iIndex)
{
    if (iIndex >= m_itemList.size())
        return QUuid();
    return m_itemList[iIndex].m_VMuid;
}

int UIResourceMonitorModel::itemIndex(const QUuid &uid)
{
    for (int i = 0; i < m_itemList.size(); ++i)
    {
        if (m_itemList[i].m_VMuid == uid)
            return i;
    }
    return -1;
}

QVariant UIResourceMonitorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole || index.row() >= rowCount())
        return QVariant();
    return m_itemList[index.row()].m_columnData[index.column()];
}

QVariant UIResourceMonitorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        return m_columnTitles.value((VMResourceMonitorColumn)section, QString());;
    return QVariant();
}

void UIResourceMonitorModel::setColumnCaptions(const QMap<int, QString>& captions)
{
    m_columnTitles = captions;
}

void UIResourceMonitorModel::initializeItems()
{
    foreach (const CMachine &comMachine, uiCommon().virtualBox().GetMachines())
    {
        if (!comMachine.isNull())
        {
            if (comMachine.GetState() == KMachineState_Running)
                addItem(comMachine.GetId(), comMachine.GetName());
        }
    }
    setupPerformanceCollector();
}

void UIResourceMonitorModel::sltMachineStateChanged(const QUuid &uId, const KMachineState state)
{
    int iIndex = itemIndex(uId);
    /* Remove the machine in case machine is no longer working. */
    if (iIndex != -1 && state != KMachineState_Running)
    {
        emit layoutAboutToBeChanged();
        removeItem(uId);
        emit layoutChanged();
        setupPerformanceCollector();
        return;
    }
    /* Insert the machine if it is working. */
    if (iIndex == -1 && state == KMachineState_Running)
    {
        emit layoutAboutToBeChanged();
        CMachine comMachine = uiCommon().virtualBox().FindMachine(uId.toString());
        if (!comMachine.isNull())
            addItem(uId, comMachine.GetName());
        emit layoutChanged();
        setupPerformanceCollector();
        return;
    }
}

void UIResourceMonitorModel::getHostRAMStats()
{
    CHost comHost = uiCommon().host();
    m_hostStats.m_iRAMTotal = _1M * (quint64)comHost.GetMemorySize();
    m_hostStats.m_iRAMFree = _1M * (quint64)comHost.GetMemoryAvailable();
}

void UIResourceMonitorModel::sltTimeout()
{
    if (!m_fShouldUpdate)
        return;
    ULONG aPctExecuting;
    ULONG aPctHalted;
    ULONG aPctVMM;

    bool fCPUColumns = columnVisible(VMResourceMonitorColumn_CPUVMMLoad) || columnVisible(VMResourceMonitorColumn_CPUGuestLoad);
    bool fNetworkColumns = columnVisible(VMResourceMonitorColumn_NetworkUpRate)
        || columnVisible(VMResourceMonitorColumn_NetworkDownRate)
        || columnVisible(VMResourceMonitorColumn_NetworkUpTotal)
        || columnVisible(VMResourceMonitorColumn_NetworkDownTotal);
    bool fIOColumns = columnVisible(VMResourceMonitorColumn_DiskIOReadRate)
        || columnVisible(VMResourceMonitorColumn_DiskIOWriteRate)
        || columnVisible(VMResourceMonitorColumn_DiskIOReadTotal)
        || columnVisible(VMResourceMonitorColumn_DiskIOWriteTotal);
    bool fVMExitColumn = columnVisible(VMResourceMonitorColumn_VMExits);

    /* Host's RAM usage is obtained from IHost not from IPerformanceCollectior: */
    getHostRAMStats();

    /* RAM usage and Host Stats: */
    queryPerformanceCollector();

    for (int i = 0; i < m_itemList.size(); ++i)
    {
        if (!m_itemList[i].m_comDebugger.isNull())
        {
            /* CPU Load: */
            if (fCPUColumns)
            {
                m_itemList[i].m_comDebugger.GetCPULoad(0x7fffffff, aPctExecuting, aPctHalted, aPctVMM);
                m_itemList[i].m_uCPUGuestLoad = aPctExecuting;
                m_itemList[i].m_uCPUVMMLoad = aPctVMM;
            }
            /* Network rate: */
            if (fNetworkColumns)
            {
                quint64 uPrevDownTotal = m_itemList[i].m_uNetworkDownTotal;
                quint64 uPrevUpTotal = m_itemList[i].m_uNetworkUpTotal;
                UIMonitorCommon::getNetworkLoad(m_itemList[i].m_comDebugger,
                                                m_itemList[i].m_uNetworkDownTotal, m_itemList[i].m_uNetworkUpTotal);
                m_itemList[i].m_uNetworkDownRate = m_itemList[i].m_uNetworkDownTotal - uPrevDownTotal;
                m_itemList[i].m_uNetworkUpRate = m_itemList[i].m_uNetworkUpTotal - uPrevUpTotal;
            }
            /* IO rate: */
            if (fIOColumns)
            {
                quint64 uPrevWriteTotal = m_itemList[i].m_uDiskWriteTotal;
                quint64 uPrevReadTotal = m_itemList[i].m_uDiskReadTotal;
                UIMonitorCommon::getDiskLoad(m_itemList[i].m_comDebugger,
                                             m_itemList[i].m_uDiskWriteTotal, m_itemList[i].m_uDiskReadTotal);
                m_itemList[i].m_uDiskWriteRate = m_itemList[i].m_uDiskWriteTotal - uPrevWriteTotal;
                m_itemList[i].m_uDiskReadRate = m_itemList[i].m_uDiskReadTotal - uPrevReadTotal;
            }
            /* VM Exits: */
            if (fVMExitColumn)
            {
                quint64 uPrevVMExitsTotal = m_itemList[i].m_uVMExitTotal;
                UIMonitorCommon::getVMMExitCount(m_itemList[i].m_comDebugger, m_itemList[i].m_uVMExitTotal);
                m_itemList[i].m_uVMExitRate = m_itemList[i].m_uVMExitTotal - uPrevVMExitsTotal;
            }
        }
    }
    int iDecimalCount = 2;
    for (int i = 0; i < m_itemList.size(); ++i)
    {
        m_itemList[i].m_columnData[VMResourceMonitorColumn_Name] = m_itemList[i].m_strVMName;
        m_itemList[i].m_columnData[VMResourceMonitorColumn_CPUGuestLoad] =
            QString("%1%").arg(QString::number(m_itemList[i].m_uCPUGuestLoad));
        m_itemList[i].m_columnData[VMResourceMonitorColumn_CPUVMMLoad] =
            QString("%1%").arg(QString::number(m_itemList[i].m_uCPUVMMLoad));

        if (m_itemList[i].isWithGuestAdditions())
            m_itemList[i].m_columnData[VMResourceMonitorColumn_RAMUsedAndTotal] =
                QString("%1/%2").arg(uiCommon().formatSize(_1K * m_itemList[i].m_uUsedRAM, iDecimalCount)).
                arg(uiCommon().formatSize(_1K * m_itemList[i].m_uTotalRAM, iDecimalCount));
        else
            m_itemList[i].m_columnData[VMResourceMonitorColumn_RAMUsedAndTotal] = UIResourceMonitorWidget::tr("N/A");

        if (m_itemList[i].isWithGuestAdditions())
            m_itemList[i].m_columnData[VMResourceMonitorColumn_RAMUsedPercentage] =
                QString("%1%").arg(QString::number(m_itemList[i].m_fRAMUsagePercentage, 'f', 2));
        else
            m_itemList[i].m_columnData[VMResourceMonitorColumn_RAMUsedPercentage] = UIResourceMonitorWidget::tr("N/A");

        m_itemList[i].m_columnData[VMResourceMonitorColumn_NetworkUpRate] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uNetworkUpRate, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_NetworkDownRate] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uNetworkDownRate, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_NetworkUpTotal] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uNetworkUpTotal, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_NetworkDownTotal] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uNetworkDownTotal, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_DiskIOReadRate] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uDiskReadRate, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_DiskIOWriteRate] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uDiskWriteRate, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_DiskIOReadTotal] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uDiskReadTotal, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_DiskIOWriteTotal] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uDiskWriteTotal, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_VMExits] =
            QString("%1/%2").arg(UICommon::addMetricSuffixToNumber(m_itemList[i].m_uVMExitRate)).
            arg(UICommon::addMetricSuffixToNumber(m_itemList[i].m_uVMExitTotal));
    }

    for (int i = 0; i < (int)VMResourceMonitorColumn_Max; ++i)
    {
        for (int j = 0; j < m_itemList.size(); ++j)
            if (m_columnDataMaxLength.value(i, 0) < m_itemList[j].m_columnData[i].length())
                m_columnDataMaxLength[i] = m_itemList[j].m_columnData[i].length();
    }
    emit sigDataUpdate();
    emit sigHostStatsUpdate(m_hostStats);
}

void UIResourceMonitorModel::setupPerformanceCollector()
{
    m_nameList.clear();
    m_objectList.clear();
    /* Initialize and configure CPerformanceCollector: */
    const ULONG iPeriod = 1;
    const int iMetricSetupCount = 1;
    if (m_performanceMonitor.isNull())
        m_performanceMonitor = uiCommon().virtualBox().GetPerformanceCollector();
    for (int i = 0; i < m_itemList.size(); ++i)
        m_nameList << "Guest/RAM/Usage*";
    /* This is for the host: */
    m_nameList << "CPU*";
    m_nameList << "FS*";
    m_objectList = QVector<CUnknown>(m_nameList.size(), CUnknown());
    m_performanceMonitor.SetupMetrics(m_nameList, m_objectList, iPeriod, iMetricSetupCount);
}

void UIResourceMonitorModel::queryPerformanceCollector()
{
    QVector<QString>  aReturnNames;
    QVector<CUnknown>  aReturnObjects;
    QVector<QString>  aReturnUnits;
    QVector<ULONG>  aReturnScales;
    QVector<ULONG>  aReturnSequenceNumbers;
    QVector<ULONG>  aReturnDataIndices;
    QVector<ULONG>  aReturnDataLengths;

    QVector<LONG> returnData = m_performanceMonitor.QueryMetricsData(m_nameList,
                                                                     m_objectList,
                                                                     aReturnNames,
                                                                     aReturnObjects,
                                                                     aReturnUnits,
                                                                     aReturnScales,
                                                                     aReturnSequenceNumbers,
                                                                     aReturnDataIndices,
                                                                     aReturnDataLengths);
    /* Parse the result we get from CPerformanceCollector to get respective values: */
    for (int i = 0; i < aReturnNames.size(); ++i)
    {
        if (aReturnDataLengths[i] == 0)
            continue;
        /* Read the last of the return data disregarding the rest since we are caching the data in GUI side: */
        float fData = returnData[aReturnDataIndices[i] + aReturnDataLengths[i] - 1] / (float)aReturnScales[i];
        if (aReturnNames[i].contains("RAM", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
        {
            if (aReturnNames[i].contains("Total", Qt::CaseInsensitive) || aReturnNames[i].contains("Free", Qt::CaseInsensitive))
            {
                {
                    CMachine comMachine = (CMachine)aReturnObjects[i];
                    if (comMachine.isNull())
                        continue;
                    int iIndex = itemIndex(comMachine.GetId());
                    if (iIndex == -1 || iIndex >= m_itemList.size())
                        continue;
                    if (aReturnNames[i].contains("Total", Qt::CaseInsensitive))
                        m_itemList[iIndex].m_uTotalRAM = fData;
                    else
                        m_itemList[iIndex].m_uFreeRAM = fData;
                }
            }
        }
        else if (aReturnNames[i].contains("CPU/Load/User", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
        {
            CHost comHost = (CHost)aReturnObjects[i];
            if (!comHost.isNull())
                m_hostStats.m_iCPUUserLoad = fData;
        }
        else if (aReturnNames[i].contains("CPU/Load/Kernel", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
        {
            CHost comHost = (CHost)aReturnObjects[i];
            if (!comHost.isNull())
                m_hostStats.m_iCPUKernelLoad = fData;
        }
        else if (aReturnNames[i].contains("CPU/MHz", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
        {
            CHost comHost = (CHost)aReturnObjects[i];
            if (!comHost.isNull())
                m_hostStats.m_iCPUFreq = fData;
        }
       else if (aReturnNames[i].contains("FS", Qt::CaseInsensitive) &&
                aReturnNames[i].contains("Total", Qt::CaseInsensitive) &&
                !aReturnNames[i].contains(":"))
       {
            CHost comHost = (CHost)aReturnObjects[i];
            if (!comHost.isNull())
                m_hostStats.m_iFSTotal = _1M * fData;
       }
       else if (aReturnNames[i].contains("FS", Qt::CaseInsensitive) &&
                aReturnNames[i].contains("Free", Qt::CaseInsensitive) &&
                !aReturnNames[i].contains(":"))
       {
            CHost comHost = (CHost)aReturnObjects[i];
            if (!comHost.isNull())
                m_hostStats.m_iFSFree = _1M * fData;
       }
    }
    for (int i = 0; i < m_itemList.size(); ++i)
    {
        m_itemList[i].m_uUsedRAM = m_itemList[i].m_uTotalRAM - m_itemList[i].m_uFreeRAM;
        if (m_itemList[i].m_uTotalRAM != 0)
            m_itemList[i].m_fRAMUsagePercentage = 100.f * (m_itemList[i].m_uUsedRAM / (float)m_itemList[i].m_uTotalRAM);
    }
}

void UIResourceMonitorModel::addItem(const QUuid& uMachineId, const QString& strMachineName)
{
    m_itemList.append(UIResourceMonitorItem(uMachineId, strMachineName));
}

void UIResourceMonitorModel::removeItem(const QUuid& uMachineId)
{
    int iIndex = itemIndex(uMachineId);
    if (iIndex == -1)
        return;
    m_itemList.remove(iIndex);
}

void UIResourceMonitorModel::setColumnVisible(const QMap<int, bool>& columnVisible)
{
    m_columnVisible = columnVisible;
}

bool UIResourceMonitorModel::columnVisible(int iColumnId) const
{
    return m_columnVisible.value(iColumnId, true);
}


/*********************************************************************************************************************************
*   Class UIResourceMonitorWidget implementation.                                                                                *
*********************************************************************************************************************************/

UIResourceMonitorWidget::UIResourceMonitorWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                                 bool fShowToolbar /* = true */, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_pToolBar(0)
    , m_pTableView(0)
    , m_pProxyModel(0)
    , m_pModel(0)
    , m_pColumnVisibilityToggleMenu(0)
    , m_pHostStatsWidget(0)
    , m_pShowPerformanceMonitorAction(0)
    , m_fIsCurrentTool(true)
    , m_iSortIndicatorWidth(0)
{
    prepare();
}

UIResourceMonitorWidget::~UIResourceMonitorWidget()
{
    saveSettings();
}

QMenu *UIResourceMonitorWidget::menu() const
{
    return NULL;
}

QMenu *UIResourceMonitorWidget::columnVisiblityToggleMenu() const
{
    return m_pColumnVisibilityToggleMenu;
}

bool UIResourceMonitorWidget::isCurrentTool() const
{
    return m_fIsCurrentTool;
}

void UIResourceMonitorWidget::setIsCurrentTool(bool fIsCurrentTool)
{
    m_fIsCurrentTool = fIsCurrentTool;
    if (m_pModel)
        m_pModel->setShouldUpdate(fIsCurrentTool);
}

void UIResourceMonitorWidget::retranslateUi()
{
    m_columnTitles[VMResourceMonitorColumn_Name] = UIResourceMonitorWidget::tr("VM Name");
    m_columnTitles[VMResourceMonitorColumn_CPUGuestLoad] = UIResourceMonitorWidget::tr("CPU Guest");
    m_columnTitles[VMResourceMonitorColumn_CPUVMMLoad] = UIResourceMonitorWidget::tr("CPU VMM");
    m_columnTitles[VMResourceMonitorColumn_RAMUsedAndTotal] = UIResourceMonitorWidget::tr("RAM Used/Total");
    m_columnTitles[VMResourceMonitorColumn_RAMUsedPercentage] = UIResourceMonitorWidget::tr("RAM %");
    m_columnTitles[VMResourceMonitorColumn_NetworkUpRate] = UIResourceMonitorWidget::tr("Network Up Rate");
    m_columnTitles[VMResourceMonitorColumn_NetworkDownRate] = UIResourceMonitorWidget::tr("Network Down Rate");
    m_columnTitles[VMResourceMonitorColumn_NetworkUpTotal] = UIResourceMonitorWidget::tr("Network Up Total");
    m_columnTitles[VMResourceMonitorColumn_NetworkDownTotal] = UIResourceMonitorWidget::tr("Network Down Total");
    m_columnTitles[VMResourceMonitorColumn_DiskIOReadRate] = UIResourceMonitorWidget::tr("Disk Read Rate");
    m_columnTitles[VMResourceMonitorColumn_DiskIOWriteRate] = UIResourceMonitorWidget::tr("Disk Write Rate");
    m_columnTitles[VMResourceMonitorColumn_DiskIOReadTotal] = UIResourceMonitorWidget::tr("Disk Read Total");
    m_columnTitles[VMResourceMonitorColumn_DiskIOWriteTotal] = UIResourceMonitorWidget::tr("Disk Write Total");
    m_columnTitles[VMResourceMonitorColumn_VMExits] = UIResourceMonitorWidget::tr("VM Exits");

    updateColumnsMenu();

    if (m_pModel)
        m_pModel->setColumnCaptions(m_columnTitles);

    computeMinimumColumnWidths();
}

void UIResourceMonitorWidget::showEvent(QShowEvent *pEvent)
{
    if (m_pShowPerformanceMonitorAction && m_pTableView)
        m_pShowPerformanceMonitorAction->setEnabled(m_pTableView->hasSelection());

    QIWithRetranslateUI<QWidget>::showEvent(pEvent);
}

void UIResourceMonitorWidget::prepare()
{
    /* Try to guest the sort indicator's width: */
    int iIndicatorMargin = 3;
    QIcon sortIndicator = qApp->QApplication::style()->standardIcon(QStyle::SP_TitleBarUnshadeButton);
    QList<QSize> iconSizes = sortIndicator.availableSizes();
    foreach(const QSize &msize, iconSizes)
        m_iSortIndicatorWidth = qMax(m_iSortIndicatorWidth, msize.width());
    if (m_iSortIndicatorWidth == 0)
        m_iSortIndicatorWidth = 20;
    m_iSortIndicatorWidth += 2 * iIndicatorMargin;

    loadHiddenColumnList();
    prepareWidgets();
    loadSettings();
    prepareActions();
    retranslateUi();
    updateModelColumVisibilityCache();
}

void UIResourceMonitorWidget::prepareWidgets()
{
    /* Create main-layout: */
    new QVBoxLayout(this);
    if (!layout())
        return;
    /* Configure layout: */
    layout()->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
    layout()->setSpacing(10);
#else
    layout()->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif

    if (m_fShowToolbar)
        prepareToolBar();

    m_pHostStatsWidget = new UIVMResourceMonitorHostStatsWidget;
    if (m_pHostStatsWidget)
        layout()->addWidget(m_pHostStatsWidget);

    m_pModel = new UIResourceMonitorModel(this);
    m_pProxyModel = new UIResourceMonitorProxyModel(this);

    m_pTableView = new UIVMResourceMonitorTableView();
    if (m_pTableView && m_pModel && m_pProxyModel)
    {
        layout()->addWidget(m_pTableView);
        m_pProxyModel->setSourceModel(m_pModel);
        m_pTableView->setModel(m_pProxyModel);
        m_pTableView->setItemDelegate(new UIVMResourceMonitorDelegate);
        m_pTableView->setSelectionMode(QAbstractItemView::SingleSelection);
        m_pTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_pTableView->setShowGrid(false);
        m_pTableView->setContextMenuPolicy(Qt::CustomContextMenu);
        m_pTableView->horizontalHeader()->setHighlightSections(false);
        m_pTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_pTableView->verticalHeader()->setVisible(false);
        m_pTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        /* Minimize the row height: */
        m_pTableView->verticalHeader()->setDefaultSectionSize(m_pTableView->verticalHeader()->minimumSectionSize());
        m_pTableView->setAlternatingRowColors(true);
        m_pTableView->setSortingEnabled(true);
        m_pTableView->sortByColumn(0, Qt::AscendingOrder);
        connect(m_pModel, &UIResourceMonitorModel::sigDataUpdate,
                this, &UIResourceMonitorWidget::sltHandleDataUpdate);
        connect(m_pModel, &UIResourceMonitorModel::sigHostStatsUpdate,
                this, &UIResourceMonitorWidget::sltHandleHostStatsUpdate);
        connect(m_pTableView, &UIVMResourceMonitorTableView::customContextMenuRequested,
                this, &UIResourceMonitorWidget::sltHandleTableContextMenuRequest);
        connect(m_pTableView, &UIVMResourceMonitorTableView::sigSelectionChanged,
                this, &UIResourceMonitorWidget::sltHandleTableSelectionChanged);
        updateModelColumVisibilityCache();
    }
}

void UIResourceMonitorWidget::updateColumnsMenu()
{
    UIMenu *pMenu = m_pActionPool->action(UIActionIndexST_M_VMResourceMonitor_M_Columns)->menu();
    if (!pMenu)
        return;
    pMenu->clear();
    for (int i = 0; i < VMResourceMonitorColumn_Max; ++i)
    {
        QAction *pAction = pMenu->addAction(m_columnTitles[i]);
        pAction->setCheckable(true);
        if (i == (int)VMResourceMonitorColumn_Name)
            pAction->setEnabled(false);
        pAction->setData(i);
        pAction->setChecked(columnVisible(i));
        connect(pAction, &QAction::toggled, this, &UIResourceMonitorWidget::sltHandleColumnAction);
    }
}

void UIResourceMonitorWidget::prepareActions()
{
    updateColumnsMenu();
    m_pShowPerformanceMonitorAction =
        m_pActionPool->action(UIActionIndexST_M_VMResourceMonitor_S_SwitchToMachinePerformance);

    if (m_pShowPerformanceMonitorAction)
        connect(m_pShowPerformanceMonitorAction, &QAction::triggered, this, &UIResourceMonitorWidget::sltHandleShowPerformanceMonitor);
}

void UIResourceMonitorWidget::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new UIToolBar(parentWidget());
    AssertPtrReturnVoid(m_pToolBar);
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            layout()->addWidget(m_pToolBar);
        }
#else
        /* Add into layout: */
        layout()->addWidget(m_pToolBar);
#endif
    }
}

void UIResourceMonitorWidget::loadSettings()
{
}

void UIResourceMonitorWidget::loadHiddenColumnList()
{
    QStringList hiddenColumnList = gEDataManager->VMResourceMonitorHiddenColumnList();
    for (int i = (int)VMResourceMonitorColumn_Name; i < (int)VMResourceMonitorColumn_Max; ++i)
        m_columnVisible[i] = true;
    foreach(const QString& strColumn, hiddenColumnList)
        setColumnVisible((int)gpConverter->fromInternalString<VMResourceMonitorColumn>(strColumn), false);
}

void UIResourceMonitorWidget::saveSettings()
{
    QStringList hiddenColumnList;
    for (int i = 0; i < m_columnVisible.size(); ++i)
    {
        if (!columnVisible(i))
            hiddenColumnList << gpConverter->toInternalString((VMResourceMonitorColumn) i);
    }
    gEDataManager->setVMResourceMonitorHiddenColumnList(hiddenColumnList);
}

void UIResourceMonitorWidget::sltToggleColumnSelectionMenu(bool fChecked)
{
    (void)fChecked;
    if (!m_pColumnVisibilityToggleMenu)
        return;
    m_pColumnVisibilityToggleMenu->exec(this->mapToGlobal(QPoint(0,0)));
}

void UIResourceMonitorWidget::sltHandleColumnAction(bool fChecked)
{
    QAction* pSender = qobject_cast<QAction*>(sender());
    if (!pSender)
        return;
    setColumnVisible(pSender->data().toInt(), fChecked);
}

void UIResourceMonitorWidget::sltHandleHostStatsUpdate(const UIVMResourceMonitorHostStats &stats)
{
    if (m_pHostStatsWidget)
        m_pHostStatsWidget->setHostStats(stats);
}

void UIResourceMonitorWidget::sltHandleDataUpdate()
{
    computeMinimumColumnWidths();
    if (m_pProxyModel)
        m_pProxyModel->dataUpdate();
}

void UIResourceMonitorWidget::sltHandleTableContextMenuRequest(const QPoint &pos)
{
    if (!m_pTableView || !m_pTableView->hasSelection())
        return;

    QMenu menu;
    if (m_pShowPerformanceMonitorAction)
        menu.addAction(m_pShowPerformanceMonitorAction);

    menu.exec(m_pTableView->mapToGlobal(pos));
}

void UIResourceMonitorWidget::sltHandleTableSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    if (m_pShowPerformanceMonitorAction)
        m_pShowPerformanceMonitorAction->setEnabled(!selected.isEmpty());
}

void UIResourceMonitorWidget::sltHandleShowPerformanceMonitor()
{
    if (!m_pTableView || !m_pModel)
        return;
    const QUuid uMachineId = m_pModel->itemUid(m_pTableView->selectedItemIndex());
    if (uMachineId.isNull())
        return;
    emit sigSwitchToMachinePerformancePane(uMachineId);
}

void UIResourceMonitorWidget::setColumnVisible(int iColumnId, bool fVisible)
{
    if (m_columnVisible.contains(iColumnId) && m_columnVisible[iColumnId] == fVisible)
        return;
    m_columnVisible[iColumnId] = fVisible;
    updateModelColumVisibilityCache();
}

void UIResourceMonitorWidget::updateModelColumVisibilityCache()
{
    if (m_pModel)
        m_pModel->setColumnVisible(m_columnVisible);
    /* Notify the table view for the changed column visibility: */
    if (m_pTableView)
        m_pTableView->updateColumVisibility();
}

void UIResourceMonitorWidget::computeMinimumColumnWidths()
{
    if (!m_pTableView || !m_pModel)
        return;
    QFontMetrics fontMetrics(m_pTableView->font());
    const QMap<int, int> &columnDataStringLengths = m_pModel->dataLengths();
    QMap<int, int> columnWidthsInPixels;
    for (int i = 0; i < (int)VMResourceMonitorColumn_Max; ++i)
    {
        int iColumnStringWidth = columnDataStringLengths.value(i, 0);
        int iColumnTitleWidth = m_columnTitles.value(i, QString()).length();
        int iMax = iColumnStringWidth > iColumnTitleWidth ? iColumnStringWidth : iColumnTitleWidth;
        columnWidthsInPixels[i] = iMax * fontMetrics.width('x') +
            QApplication::style()->pixelMetric(QStyle::PM_LayoutLeftMargin) +
            QApplication::style()->pixelMetric(QStyle::PM_LayoutRightMargin) +
            m_iSortIndicatorWidth;
    }
    m_pTableView->setMinimumColumnWidths(columnWidthsInPixels);
}

bool UIResourceMonitorWidget::columnVisible(int iColumnId) const
{
    return m_columnVisible.value(iColumnId, true);
}


/*********************************************************************************************************************************
*   Class UIResourceMonitorFactory implementation.                                                                               *
*********************************************************************************************************************************/

UIResourceMonitorFactory::UIResourceMonitorFactory(UIActionPool *pActionPool /* = 0 */)
    : m_pActionPool(pActionPool)
{
}

void UIResourceMonitorFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UIResourceMonitor(pCenterWidget, m_pActionPool);
}


/*********************************************************************************************************************************
*   Class UIResourceMonitor implementation.                                                                                      *
*********************************************************************************************************************************/

UIResourceMonitor::UIResourceMonitor(QWidget *pCenterWidget, UIActionPool *pActionPool)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
{
}

void UIResourceMonitor::retranslateUi()
{
    setWindowTitle(UIResourceMonitorWidget::tr("VM Resource Monitor"));
}

void UIResourceMonitor::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/host_iface_manager_32px.png", ":/host_iface_manager_16px.png"));
}

void UIResourceMonitor::configureCentralWidget()
{
    UIResourceMonitorWidget *pWidget = new UIResourceMonitorWidget(EmbedTo_Dialog, m_pActionPool, true, this);
    AssertPtrReturnVoid(pWidget);
    {
        setWidget(pWidget);
        setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        setWidgetToolbar(pWidget->toolbar());
#endif
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UIResourceMonitor::configureButtonBox()
{
}

void UIResourceMonitor::finalize()
{
    retranslateUi();
}

UIResourceMonitorWidget *UIResourceMonitor::widget()
{
    return qobject_cast<UIResourceMonitorWidget*>(QIManagerDialog::widget());
}


#include "UIResourceMonitor.moc"
