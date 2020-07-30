/* $Id$ */
/** @file
 * VBox Qt GUI - UIPerformanceMonitor class implementation.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
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
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QGridLayout>
#include <QScrollArea>
#include <QStyle>
#include <QXmlStreamReader>
#include <QTimer>

/* GUI includes: */
#include "QIFileDialog.h"
#include "UICommon.h"
#include "UIPerformanceMonitor.h"
#include "UIToolBar.h"
#include "UIVirtualBoxEventHandler.h"

/* COM includes: */
#include "CConsole.h"
#include "CGuest.h"
#include "CPerformanceCollector.h"
#include "CPerformanceMetric.h"

/* External includes: */
#include <math.h>

/** The time in seconds between metric inquries done to API. */
const ULONG g_iPeriod = 1;
/** The number of data points we store in UIChart. with g_iPeriod=1 it corresponds to 2 min. of data. */
const int g_iMaximumQueueSize = 120;
/** This is passed to IPerformanceCollector during its setup. When 1 that means IPerformanceCollector object does a data cache of size 1. */
const int g_iMetricSetupCount = 1;
const int g_iDecimalCount = 2;


/*********************************************************************************************************************************
*   UIChart definition.                                                                                                          *
*********************************************************************************************************************************/

class UIChart : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

public:

    UIChart(QWidget *pParent, UIMetric *pMetric);
    void setFontSize(int iFontSize);
    int  fontSize() const;
    const QStringList &textList() const;

    bool isPieChartAllowed() const;
    void setIsPieChartAllowed(bool fWithPieChart);

    bool usePieChart() const;
    void setShowPieChart(bool fShowPieChart);

    bool useGradientLineColor() const;
    void setUseGradientLineColor(bool fUseGradintLineColor);

    bool useAreaChart() const;
    void setUseAreaChart(bool fUseAreaChart);

    bool isAreaChartAllowed() const;
    void setIsAreaChartAllowed(bool fIsAreaChartAllowed);

    QColor dataSeriesColor(int iDataSeriesIndex, int iDark = 0);
    void setDataSeriesColor(int iDataSeriesIndex, const QColor &color);

    QString XAxisLabel();
    void setXAxisLabel(const QString &strLabel);

    bool isAvailable() const;
    void setIsAvailable(bool fIsAvailable);

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */;
    virtual QSize minimumSizeHint() const /* override */;
    virtual QSize sizeHint() const  /* override */;
    virtual void retranslateUi()  /* override */;

private slots:

    void sltCreateContextMenu(const QPoint &point);
    void sltResetMetric();
    void sltSetShowPieChart(bool fShowPieChart);
    void sltSetUseAreaChart(bool fUseAreaChart);

private:

    /** @name Drawing helper functions.
     * @{ */
       void drawXAxisLabels(QPainter &painter, int iXSubAxisCount);
       void drawPieChart(QPainter &painter, quint64 iMaximum, int iDataIndex, const QRectF &chartRect, bool fWithBorder = true);
       void drawCombinedPieCharts(QPainter &painter, quint64 iMaximum);

       /** Drawing an overlay rectangle over the charts to indicate that they are disabled. */
       void drawDisabledChartRectangle(QPainter &painter);
       QConicalGradient conicalGradientForDataSeries(const QRectF &rectangle, int iDataIndex);
    /** @} */

    UIMetric *m_pMetric;
    QSize m_size;
    QFont m_axisFont;
    int m_iMarginLeft;
    int m_iMarginRight;
    int m_iMarginTop;
    int m_iMarginBottom;
    int m_iOverlayAlpha;
    QRect m_lineChartRect;
    int m_iPieChartRadius;
    int m_iPieChartSpacing;
    /** For some chart it is not possible to have a pie chart, Then We dont present the
      * option to show it to user. see m_fIsPieChartAllowed. */
    bool m_fIsPieChartAllowed;
    /**  m_fShowPieChart is considered only if m_fIsPieChartAllowed is true. */
    bool m_fShowPieChart;
    bool m_fUseGradientLineColor;
    /** When it is true we draw an area graph where data series drawn on top of each other.
     *  We draw first data0 then data 1 on top. Makes sense where the summation of data is guaranteed not to exceed some max. */
    bool m_fUseAreaChart;
    /** False if the chart is not useable for some reason. For example it depends guest additions and they are not installed. */
    bool m_fIsAvailable;
    /** For some charts it does not make sense to have an area chart. */
    bool m_fIsAreaChartAllowed;
    QColor m_dataSeriesColor[DATA_SERIES_SIZE];
    QString m_strXAxisLabel;
    QString m_strGAWarning;
    QString m_strResetActionLabel;
    QString m_strPieChartToggleActionLabel;
    QString m_strAreaChartToggleActionLabel;
};

/*********************************************************************************************************************************
*   UIChart implementation.                                                                                     *
*********************************************************************************************************************************/

UIChart::UIChart(QWidget *pParent, UIMetric *pMetric)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pMetric(pMetric)
    , m_size(QSize(50, 50))
    , m_iOverlayAlpha(80)
    , m_fIsPieChartAllowed(false)
    , m_fShowPieChart(true)
    , m_fUseGradientLineColor(false)
    , m_fUseAreaChart(true)
    , m_fIsAvailable(true)
    , m_fIsAreaChartAllowed(false)
{
    m_axisFont = font();
    m_axisFont.setPixelSize(14);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &UIChart::customContextMenuRequested,
            this, &UIChart::sltCreateContextMenu);

    m_dataSeriesColor[0] = QColor(200, 0, 0, 255);
    m_dataSeriesColor[1] = QColor(0, 0, 200, 255);

    m_iMarginLeft = 1 * qApp->QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    m_iMarginRight = 9 * QFontMetrics(m_axisFont).width('X');
    m_iMarginTop = 0.3 * qApp->QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    m_iMarginBottom = QFontMetrics(m_axisFont).height();

    float fAppIconSize = qApp->style()->pixelMetric(QStyle::PM_LargeIconSize);
    m_size = QSize(14 * fAppIconSize,  3.5 * fAppIconSize);
    m_iPieChartSpacing = 2;
    m_iPieChartRadius = m_size.height() - (m_iMarginTop + m_iMarginBottom + 2 * m_iPieChartSpacing);

    retranslateUi();
}

bool UIChart::isPieChartAllowed() const
{
    return m_fIsPieChartAllowed;
}

void UIChart::setIsPieChartAllowed(bool fWithPieChart)
{
    if (m_fIsPieChartAllowed == fWithPieChart)
        return;
    m_fIsPieChartAllowed = fWithPieChart;
    update();
}

bool UIChart::usePieChart() const
{
    return m_fShowPieChart;
}

void UIChart::setShowPieChart(bool fDrawChart)
{
    if (m_fShowPieChart == fDrawChart)
        return;
    m_fShowPieChart = fDrawChart;
    update();
}

bool UIChart::useGradientLineColor() const
{
    return m_fUseGradientLineColor;
}

void UIChart::setUseGradientLineColor(bool fUseGradintLineColor)
{
    if (m_fUseGradientLineColor == fUseGradintLineColor)
        return;
    m_fUseGradientLineColor = fUseGradintLineColor;
    update();
}

bool UIChart::useAreaChart() const
{
    return m_fUseAreaChart;
}

void UIChart::setUseAreaChart(bool fUseAreaChart)
{
    if (m_fUseAreaChart == fUseAreaChart)
        return;
    m_fUseAreaChart = fUseAreaChart;
    update();
}

bool UIChart::isAreaChartAllowed() const
{
    return m_fIsAreaChartAllowed;
}

void UIChart::setIsAreaChartAllowed(bool fIsAreaChartAllowed)
{
    m_fIsAreaChartAllowed = fIsAreaChartAllowed;
}

QColor UIChart::dataSeriesColor(int iDataSeriesIndex, int iDark /* = 0 */)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return QColor();
    return QColor(qMax(m_dataSeriesColor[iDataSeriesIndex].red() - iDark, 0),
                  qMax(m_dataSeriesColor[iDataSeriesIndex].green() - iDark, 0),
                  qMax(m_dataSeriesColor[iDataSeriesIndex].blue() - iDark, 0),
                  m_dataSeriesColor[iDataSeriesIndex].alpha());
}

void UIChart::setDataSeriesColor(int iDataSeriesIndex, const QColor &color)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return;
    if (m_dataSeriesColor[iDataSeriesIndex] == color)
        return;
    m_dataSeriesColor[iDataSeriesIndex] = color;
    update();
}

QString UIChart::XAxisLabel()
{
    return m_strXAxisLabel;
}

void UIChart::setXAxisLabel(const QString &strLabel)
{
    m_strXAxisLabel = strLabel;
}

bool UIChart::isAvailable() const
{
    return m_fIsAvailable;
}

void UIChart::setIsAvailable(bool fIsAvailable)
{
    if (m_fIsAvailable == fIsAvailable)
        return;
    m_fIsAvailable = fIsAvailable;
    update();
}

QSize UIChart::minimumSizeHint() const
{
    return m_size;
}

QSize UIChart::sizeHint() const
{
    return m_size;
}

void UIChart::retranslateUi()
{
    m_strGAWarning = QApplication::translate("UIVMInformationDialog", "This metric requires guest additions to work.");
    m_strResetActionLabel = QApplication::translate("UIVMInformationDialog", "Reset");
    m_strPieChartToggleActionLabel = QApplication::translate("UIVMInformationDialog", "Show Pie Chart");
    m_strAreaChartToggleActionLabel = QApplication::translate("UIVMInformationDialog", "Draw Area Chart");
    update();
}

void UIChart::paintEvent(QPaintEvent *pEvent)
{
    Q_UNUSED(pEvent);
    if (!m_pMetric || g_iMaximumQueueSize <= 1)
        return;

    QPainter painter(this);
    painter.setFont(m_axisFont);
    painter.setRenderHint(QPainter::Antialiasing);

    /* Draw a rectanglar grid over which we will draw the line graphs: */
    QPoint chartTopLeft(m_iMarginLeft, m_iMarginTop);
    QSize chartSize(width() - (m_iMarginLeft + m_iMarginRight), height() - (m_iMarginTop + m_iMarginBottom));

    m_lineChartRect = QRect(chartTopLeft, chartSize);
    QColor mainAxisColor(120, 120, 120);
    QColor subAxisColor(200, 200, 200);
    /* Draw the main axes: */
    painter.setPen(mainAxisColor);
    painter.drawRect(m_lineChartRect);

    /* draw Y subaxes: */
    painter.setPen(subAxisColor);
    int iYSubAxisCount = 3;
    for (int i = 0; i < iYSubAxisCount; ++i)
    {
        float fSubAxisY = m_iMarginTop + (i + 1) * m_lineChartRect.height() / (float) (iYSubAxisCount + 1);
        painter.drawLine(m_lineChartRect.left(), fSubAxisY,
                         m_lineChartRect.right(), fSubAxisY);
    }

    /* draw X subaxes: */
    int iXSubAxisCount = 5;
    for (int i = 0; i < iXSubAxisCount; ++i)
    {
        float fSubAxisX = m_lineChartRect.left() + (i + 1) * m_lineChartRect.width() / (float) (iXSubAxisCount + 1);
        painter.drawLine(fSubAxisX, m_lineChartRect.top(), fSubAxisX, m_lineChartRect.bottom());
    }

    /* Draw XAxis tick labels: */
    painter.setPen(mainAxisColor);
    drawXAxisLabels(painter, iXSubAxisCount);

    if (!isEnabled())
        return;

    /* Draw a half-transparent rectangle over the whole widget to indicate the it is not available: */
    if (!isAvailable())
    {
        drawDisabledChartRectangle(painter);
        return;
    }

    quint64 iMaximum = m_pMetric->maximum();
    if (iMaximum == 0)
        return;
    /* Draw the data lines: */
    float fBarWidth = m_lineChartRect.width() / (float) (g_iMaximumQueueSize - 1);
    float fH = m_lineChartRect.height() / (float)iMaximum;
    for (int k = 0; k < DATA_SERIES_SIZE; ++k)
    {
        if (m_fUseGradientLineColor)
        {
            QLinearGradient gradient(0, 0, 0, m_lineChartRect.height());
            gradient.setColorAt(0, Qt::black);
            gradient.setColorAt(1, m_dataSeriesColor[k]);
            painter.setPen(QPen(gradient, 2.5));
        }

        const QQueue<quint64> *data = m_pMetric->data(k);
        if (!m_fUseGradientLineColor)
            painter.setPen(QPen(m_dataSeriesColor[k], 2.5));
        if (m_fUseAreaChart && m_fIsAreaChartAllowed)
        {
            QVector<QPointF> points;
            for (int i = 0; i < data->size(); ++i)
            {
                float fHeight = fH * data->at(i);
                if (k == 0)
                {
                    if (m_pMetric->data(1) && m_pMetric->data(1)->size() > i)
                        fHeight += fH * m_pMetric->data(1)->at(i);
                }
                float fX = (width() - m_iMarginRight) - ((data->size() - i - 1) * fBarWidth);
                if (i == 0)
                    points << QPointF(fX, height() - m_iMarginBottom);
                points << QPointF(fX, height() - (fHeight + m_iMarginBottom));
                if (i == data->size() - 1)
                    points << QPointF(fX, height() - + m_iMarginBottom);
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_dataSeriesColor[k]);
            painter.drawPolygon(points, Qt::WindingFill);
        }
        else
        {
            for (int i = 0; i < data->size() - 1; ++i)
            {
                int j = i + 1;
                float fHeight = fH * data->at(i);
                float fX = (width() - m_iMarginRight) - ((data->size() -i - 1) * fBarWidth);
                float fHeight2 = fH * data->at(j);
                float fX2 = (width() - m_iMarginRight) - ((data->size() -j - 1) * fBarWidth);
                QLineF bar(fX, height() - (fHeight + m_iMarginBottom), fX2, height() - (fHeight2 + m_iMarginBottom));
                painter.drawLine(bar);
            }
        }
    }

    QFontMetrics fontMetrics(painter.font());
    int iFontHeight = fontMetrics.height();

    /* Draw YAxis tick labels: */
    painter.setPen(mainAxisColor);
    for (int i = 0; i < iYSubAxisCount + 2; ++i)
    {
        int iTextY = 0.5 * iFontHeight + m_iMarginTop + i * m_lineChartRect.height() / (float) (iYSubAxisCount + 1);
        QString strValue;
        quint64 iValue = (iYSubAxisCount + 1 - i) * (iMaximum / (float) (iYSubAxisCount + 1));
        if (m_pMetric->unit().compare("%", Qt::CaseInsensitive) == 0)
            strValue = QString::number(iValue);
        else if (m_pMetric->unit().compare("kb", Qt::CaseInsensitive) == 0)
            strValue = uiCommon().formatSize(_1K * (quint64)iValue, g_iDecimalCount);
        else if (m_pMetric->unit().compare("b", Qt::CaseInsensitive) == 0 ||
                 m_pMetric->unit().compare("b/s", Qt::CaseInsensitive) == 0)
            strValue = uiCommon().formatSize(iValue, g_iDecimalCount);
        else if (m_pMetric->unit().compare("times", Qt::CaseInsensitive) == 0)
            strValue = UICommon::addMetricSuffixToNumber(iValue);

        painter.drawText(width() - 0.9 * m_iMarginRight, iTextY, strValue);
    }

    if (m_fIsPieChartAllowed && m_fShowPieChart)
        drawCombinedPieCharts(painter, iMaximum);
}

void UIChart::drawXAxisLabels(QPainter &painter, int iXSubAxisCount)
{
    QFont painterFont = painter.font();
    QFontMetrics fontMetrics(painter.font());
    int iFontHeight = fontMetrics.height();

    int iTotalSeconds = g_iPeriod * g_iMaximumQueueSize;
    for (int i = 0; i < iXSubAxisCount + 2; ++i)
    {
        int iTextX = m_lineChartRect.left() + i * m_lineChartRect.width() / (float) (iXSubAxisCount + 1);
        QString strCurrentSec = QString::number(iTotalSeconds - i * iTotalSeconds / (float)(iXSubAxisCount + 1));
        int iTextWidth = fontMetrics.width(strCurrentSec);
        if (i == 0)
        {
            strCurrentSec += " " + m_strXAxisLabel;
            painter.drawText(iTextX, m_lineChartRect.bottom() + iFontHeight, strCurrentSec);
        }
        else
            painter.drawText(iTextX - 0.5 * iTextWidth, m_lineChartRect.bottom() + iFontHeight, strCurrentSec);
    }
}

void UIChart::drawPieChart(QPainter &painter, quint64 iMaximum, int iDataIndex,
                           const QRectF &chartRect, bool fWithBorder /* = false */)
{
    if (!m_pMetric)
        return;

    const QQueue<quint64> *data = m_pMetric->data(iDataIndex);
    if (!data || data->isEmpty())
        return;

    /* Draw a whole non-filled circle: */
    if (fWithBorder)
    {
        painter.setPen(QPen(QColor(100, 100, 100, m_iOverlayAlpha), 1));
        painter.drawArc(chartRect, 0, 3600 * 16);
        painter.setPen(Qt::NoPen);
    }

   /* Draw a white filled circle and that the arc for data: */
    QPainterPath background = UIMonitorCommon::wholeArc(chartRect);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, m_iOverlayAlpha));
    painter.drawPath(background);

    float fAngle = 360.f * data->back() / (float)iMaximum;

    QPainterPath dataPath;
    dataPath.moveTo(chartRect.center());
    dataPath.arcTo(chartRect, 90.f/*startAngle*/,
                   -1.f * fAngle /*sweepLength*/);
    painter.setBrush(conicalGradientForDataSeries(chartRect, iDataIndex));
    painter.drawPath(dataPath);
}

QConicalGradient UIChart::conicalGradientForDataSeries(const QRectF &rectangle, int iDataIndex)
{
    QConicalGradient gradient;
    gradient.setCenter(rectangle.center());
    gradient.setAngle(90);
    gradient.setColorAt(0, QColor(0, 0, 0, m_iOverlayAlpha));
    QColor pieColor(m_dataSeriesColor[iDataIndex]);
    pieColor.setAlpha(m_iOverlayAlpha);
    gradient.setColorAt(1, pieColor);
    return gradient;
}

void UIChart::drawCombinedPieCharts(QPainter &painter, quint64 iMaximum)
{
    if (!m_pMetric)
        return;

    QRectF chartRect(QPointF(m_iPieChartSpacing + m_iMarginLeft, m_iPieChartSpacing + m_iMarginTop),
                     QSizeF(m_iPieChartRadius, m_iPieChartRadius));

    bool fData0 = m_pMetric->data(0) && !m_pMetric->data(0)->isEmpty();
    bool fData1 = m_pMetric->data(0) && !m_pMetric->data(1)->isEmpty();

    if (fData0 && fData1)
    {
        /* Draw a doughnut chart where data series are stacked on to of each other: */
        if (m_pMetric->data(0) && !m_pMetric->data(0)->isEmpty() &&
            m_pMetric->data(1) && !m_pMetric->data(1)->isEmpty())
            UIMonitorCommon::drawCombinedDoughnutChart(m_pMetric->data(1)->back(), dataSeriesColor(1, 50),
                                                       m_pMetric->data(0)->back(), dataSeriesColor(0, 50),
                                                       painter, iMaximum, chartRect,
                                                       UIMonitorCommon::getScaledRect(chartRect, 0.5f, 0.5f), m_iOverlayAlpha);
#if 0
        /* Draw a doughnut shaped chart and then pie chart inside it: */
        UIMonitorCommon::drawDoughnutChart(painter, iMaximum, m_pMetric->data(0)->back(),
                                           chartRect, innerRect, m_iOverlayAlpha, dataSeriesColor(0));
        drawPieChart(painter, iMaximum, 1 /* iDataIndex */, innerRect, false);
#endif
    }
    else if (fData0 && !fData1)
        drawPieChart(painter, iMaximum, 0 /* iDataIndex */, chartRect);
    else if (!fData0 && fData1)
        drawPieChart(painter, iMaximum, 1 /* iDataIndex */, chartRect);
}

void UIChart::drawDisabledChartRectangle(QPainter &painter)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 150));
    painter.drawRect(m_lineChartRect);
    painter.setPen(QColor(20, 20, 20, 180));
    QFont font = painter.font();
    int iFontSize = 64;
    do {
        font.setPixelSize(iFontSize);
        --iFontSize;
    }while(QFontMetrics(font).width(m_strGAWarning) >= 0.8 * m_lineChartRect.width());
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(m_lineChartRect, m_strGAWarning);
}

void UIChart::sltCreateContextMenu(const QPoint &point)
{
    QMenu menu;
    QAction *pResetAction = menu.addAction(m_strResetActionLabel);
    connect(pResetAction, &QAction::triggered, this, &UIChart::sltResetMetric);
    if (m_fIsPieChartAllowed)
    {
        QAction *pPieChartToggle = menu.addAction(m_strPieChartToggleActionLabel);
        pPieChartToggle->setCheckable(true);
        pPieChartToggle->setChecked(m_fShowPieChart);
        connect(pPieChartToggle, &QAction::toggled, this, &UIChart::sltSetShowPieChart);
    }
    if (m_fIsAreaChartAllowed)
    {
        QAction *pAreaChartToggle = menu.addAction(m_strAreaChartToggleActionLabel);
        pAreaChartToggle->setCheckable(true);
        pAreaChartToggle->setChecked(m_fUseAreaChart);
        connect(pAreaChartToggle, &QAction::toggled, this, &UIChart::sltSetUseAreaChart);
    }

    menu.exec(mapToGlobal(point));
}

void UIChart::sltResetMetric()
{
    if (m_pMetric)
        m_pMetric->reset();
}

void UIChart::sltSetShowPieChart(bool fShowPieChart)
{
    setShowPieChart(fShowPieChart);
}

void UIChart::sltSetUseAreaChart(bool fUseAreaChart)
{
    setUseAreaChart(fUseAreaChart);
}


/*********************************************************************************************************************************
*   UIMetric implementation.                                                                                                     *
*********************************************************************************************************************************/

UIMetric::UIMetric(const QString &strName, const QString &strUnit, int iMaximumQueueSize)
    : m_strName(strName)
    , m_strUnit(strUnit)
    , m_iMaximum(0)
#if 0 /* Unused according to Clang 11. */
    , m_iMaximumQueueSize(iMaximumQueueSize) /** @todo r=bird: m_iMaximumQueueSize is not used anywhere that I can see. */
#endif
    , m_fRequiresGuestAdditions(false)
    , m_fIsInitialized(false)
{
    RT_NOREF(iMaximumQueueSize); /* Unused according to Clang 11. */
    m_iTotal[0] = 0;
    m_iTotal[1] = 0;
}

UIMetric::UIMetric()
    : m_iMaximum(0)
#if 0 /* Unused according to Clang 11. */
    , m_iMaximumQueueSize(0)
#endif
    , m_fRequiresGuestAdditions(false)
    , m_fIsInitialized(false)
{
}

const QString &UIMetric::name() const
{
    return m_strName;
}

void UIMetric::setMaximum(quint64 iMaximum)
{
    m_iMaximum = iMaximum;
}

quint64 UIMetric::maximum() const
{
    return m_iMaximum;
}

void UIMetric::setUnit(QString strUnit)
{
    m_strUnit = strUnit;

}
const QString &UIMetric::unit() const
{
    return m_strUnit;
}

void UIMetric::addData(int iDataSeriesIndex, quint64 fData)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return;
    m_data[iDataSeriesIndex].enqueue(fData);
    if (m_data[iDataSeriesIndex].size() > g_iMaximumQueueSize)
        m_data[iDataSeriesIndex].dequeue();
}

const QQueue<quint64> *UIMetric::data(int iDataSeriesIndex) const
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return 0;
    return &m_data[iDataSeriesIndex];
}

void UIMetric::setTotal(int iDataSeriesIndex, quint64 iTotal)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return;
    m_iTotal[iDataSeriesIndex] = iTotal;
}

quint64 UIMetric::total(int iDataSeriesIndex) const
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return 0;
    return m_iTotal[iDataSeriesIndex];
}

bool UIMetric::requiresGuestAdditions() const
{
    return m_fRequiresGuestAdditions;
}

void UIMetric::setRequiresGuestAdditions(bool fRequiresGAs)
{
    m_fRequiresGuestAdditions = fRequiresGAs;
}

bool UIMetric::isInitialized() const
{
    return m_fIsInitialized;
}

void UIMetric::setIsInitialized(bool fIsInitialized)
{
    m_fIsInitialized = fIsInitialized;
}

void UIMetric::reset()
{
    m_fIsInitialized = false;
    for (int i = 0; i < DATA_SERIES_SIZE; ++i)
    {
        m_iTotal[i] = 0;
        m_data[i].clear();
    }
    m_iMaximum = 0;
}

void UIMetric::toFile(QFile &file) const
{
    QTextStream stream(&file);
    stream << m_strName << "\n";
    foreach (const quint64& data, m_data[0])
        stream << data;
}

/*********************************************************************************************************************************
*   UIPerformanceMonitor implementation.                                                                              *
*********************************************************************************************************************************/

UIPerformanceMonitor::UIPerformanceMonitor(EmbedTo enmEmbedding, QWidget *pParent,
                                           const CMachine &machine, bool fShowToolbar /* = false */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fGuestAdditionsAvailable(false)
    , m_pMainLayout(0)
    , m_pTimer(0)
    , m_pToolBar(0)
    , m_strCPUMetricName("CPU Load")
    , m_strRAMMetricName("RAM Usage")
    , m_strDiskMetricName("Disk Usage")
    , m_strNetworkMetricName("Network")
    , m_strDiskIOMetricName("DiskIO")
    , m_strVMExitMetricName("VMExits")
    , m_iTimeStep(0)
    , m_enmEmbedding(enmEmbedding)
    , m_fShowToolbar(fShowToolbar)
{
    prepareMetrics();
    prepareWidgets();
    if (fShowToolbar)
        prepareToolBar();
    retranslateUi();
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange, this, &UIPerformanceMonitor::sltMachineStateChange);
    setMachine(machine);
}

UIPerformanceMonitor::~UIPerformanceMonitor()
{
}

void UIPerformanceMonitor::setMachine(const CMachine &comMachine)
{
    reset();
    if (comMachine.isNull())
        return;

    if (!m_comSession.isNull() && m_comSession.GetState() == KSessionState_Locked)
        m_comSession.UnlockMachine();

    m_comMachine = comMachine;

    if (m_comMachine.GetState() == KMachineState_Running)
    {
        setEnabled(true);
        openSession();
        start();
    }
}

void UIPerformanceMonitor::openSession()
{
    m_comSession = uiCommon().openSession(m_comMachine.GetId(), KLockType_Shared);
    AssertReturnVoid(!m_comSession.isNull());

    CConsole comConsole = m_comSession.GetConsole();
    AssertReturnVoid(!comConsole.isNull());
    m_comGuest = comConsole.GetGuest();

    m_comMachineDebugger = comConsole.GetDebugger();
}

void UIPerformanceMonitor::retranslateUi()
{
    foreach (UIChart *pChart, m_charts)
        pChart->setXAxisLabel(QApplication::translate("UIVMInformationDialog", "Seconds"));

    /* Translate the chart info labels: */
    int iMaximum = 0;
    m_strCPUInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "CPU Load");
    iMaximum = qMax(iMaximum, m_strCPUInfoLabelTitle.length());
    m_strCPUInfoLabelGuest = QApplication::translate("UIVMInformationDialog", "Guest Load");
    iMaximum = qMax(iMaximum, m_strCPUInfoLabelGuest.length());
    m_strCPUInfoLabelVMM = QApplication::translate("UIVMInformationDialog", "VMM Load");
    iMaximum = qMax(iMaximum, m_strCPUInfoLabelVMM.length());
    m_strRAMInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "RAM Usage");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelTitle.length());
    m_strRAMInfoLabelTotal = QApplication::translate("UIVMInformationDialog", "Total");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelTotal.length());
    m_strRAMInfoLabelFree = QApplication::translate("UIVMInformationDialog", "Free");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelFree.length());
    m_strRAMInfoLabelUsed = QApplication::translate("UIVMInformationDialog", "Used");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelUsed.length());
    m_strNetworkInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "Network Rate");
    iMaximum = qMax(iMaximum, m_strNetworkInfoLabelTitle.length());
    m_strNetworkInfoLabelReceived = QApplication::translate("UIVMInformationDialog", "Receive Rate");
    iMaximum = qMax(iMaximum, m_strNetworkInfoLabelReceived.length());
    m_strNetworkInfoLabelTransmitted = QApplication::translate("UIVMInformationDialog", "Transmit Rate");
    iMaximum = qMax(iMaximum, m_strNetworkInfoLabelTransmitted.length());
    m_strNetworkInfoLabelReceivedTotal = QApplication::translate("UIVMInformationDialog", "Total Received");
    iMaximum = qMax(iMaximum, m_strNetworkInfoLabelReceivedTotal.length());
    m_strNetworkInfoLabelTransmittedTotal = QApplication::translate("UIVMInformationDialog", "Total Transmitted");
    iMaximum = qMax(iMaximum, m_strNetworkInfoLabelReceivedTotal.length());
    m_strDiskIOInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "Disk IO Rate");
    iMaximum = qMax(iMaximum, m_strDiskIOInfoLabelTitle.length());
    m_strDiskIOInfoLabelWritten = QApplication::translate("UIVMInformationDialog", "Write Rate");
    iMaximum = qMax(iMaximum, m_strDiskIOInfoLabelWritten.length());
    m_strDiskIOInfoLabelRead = QApplication::translate("UIVMInformationDialog", "Read Rate");
    iMaximum = qMax(iMaximum, m_strDiskIOInfoLabelRead.length());
    m_strDiskIOInfoLabelWrittenTotal = QApplication::translate("UIVMInformationDialog", "Total Written");
    iMaximum = qMax(iMaximum, m_strDiskIOInfoLabelWrittenTotal.length());
    m_strDiskIOInfoLabelReadTotal = QApplication::translate("UIVMInformationDialog", "Total Read");
    iMaximum = qMax(iMaximum, m_strDiskIOInfoLabelReadTotal.length());
    m_strVMExitInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "VM Exits");
    iMaximum = qMax(iMaximum, m_strVMExitInfoLabelTitle.length());
    m_strVMExitLabelCurrent = QApplication::translate("UIVMInformationDialog", "Current");
    iMaximum = qMax(iMaximum, m_strVMExitLabelCurrent.length());
    m_strVMExitLabelTotal = QApplication::translate("UIVMInformationDialog", "Total");
    iMaximum = qMax(iMaximum, m_strVMExitLabelTotal.length());

    /* Compute the maximum label string length and set it as a fixed width to labels to prevent always changing widths: */
    /* Add m_iDecimalCount plus 4 characters for the number and 3 for unit string: */
    iMaximum += (g_iDecimalCount + 7);
    if (!m_infoLabels.isEmpty())
    {
        QLabel *pLabel = m_infoLabels.begin().value();
        if (pLabel)
        {
            QFontMetrics labelFontMetric(pLabel->font());
            int iWidth = iMaximum * labelFontMetric.width('X');
            foreach (QLabel *pInfoLabel, m_infoLabels)
                pInfoLabel->setFixedWidth(iWidth);
        }
    }
}

void UIPerformanceMonitor::prepareWidgets()
{
    m_pMainLayout = new QVBoxLayout(this);
    if (!m_pMainLayout)
        return;

    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
    m_pMainLayout->setSpacing(10);
#else
    m_pMainLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif

    m_pTimer = new QTimer(this);
    if (m_pTimer)
        connect(m_pTimer, &QTimer::timeout, this, &UIPerformanceMonitor::sltTimeout);

    QScrollArea *pScrollArea = new QScrollArea(this);
    m_pMainLayout->addWidget(pScrollArea);

    QWidget *pContainerWidget = new QWidget(pScrollArea);
    QGridLayout *pContainerLayout = new QGridLayout(pContainerWidget);
    pContainerWidget->setLayout(pContainerLayout);
    pContainerLayout->setSpacing(10);
    pContainerWidget->show();
    pScrollArea->setWidget(pContainerWidget);
    pScrollArea->setWidgetResizable(true);

    QStringList chartOrder;
    chartOrder << m_strCPUMetricName << m_strRAMMetricName <<
        m_strDiskMetricName << m_strNetworkMetricName << m_strDiskIOMetricName << m_strVMExitMetricName;
    int iRow = 0;
    foreach (const QString &strMetricName, chartOrder)
    {
        QHBoxLayout *pChartLayout = new QHBoxLayout;
        pChartLayout->setSpacing(0);

        if (!m_metrics.contains(strMetricName))
            continue;
        QLabel *pLabel = new QLabel(this);
        pLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        pChartLayout->addWidget(pLabel);
        m_infoLabels.insert(strMetricName, pLabel);

        UIChart *pChart = new UIChart(this, &(m_metrics[strMetricName]));
        m_charts.insert(strMetricName, pChart);
        pChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        pChartLayout->addWidget(pChart);
        pContainerLayout->addLayout(pChartLayout, iRow, 0, 1, 2);
        ++iRow;
    }

    /* Configure charts: */
    if (m_charts.contains(m_strCPUMetricName) && m_charts[m_strCPUMetricName])
    {
        m_charts[m_strCPUMetricName]->setIsPieChartAllowed(true);
        m_charts[m_strCPUMetricName]->setIsAreaChartAllowed(true);
    }

    QWidget *bottomSpacerWidget = new QWidget(this);
    bottomSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    bottomSpacerWidget->setVisible(true);
    pContainerLayout->addWidget(bottomSpacerWidget, iRow, 0, 1, 2);
}

void UIPerformanceMonitor::sltTimeout()
{
    if (m_performanceMonitor.isNull())
        return;
    ++m_iTimeStep;

    if (m_metrics.contains(m_strRAMMetricName))
    {
        quint64 iTotalRAM = 0;
        quint64 iFreeRAM = 0;
        UIMonitorCommon::getRAMLoad(m_performanceMonitor, m_nameList, m_objectList, iTotalRAM, iFreeRAM);
        updateRAMGraphsAndMetric(iTotalRAM, iFreeRAM);
    }

    /* Update the CPU load chart with values we get from IMachineDebugger::getCPULoad(..): */
    if (m_metrics.contains(m_strCPUMetricName))
    {
        ULONG aPctExecuting;
        ULONG aPctHalted;
        ULONG aPctOther;
        m_comMachineDebugger.GetCPULoad(0x7fffffff, aPctExecuting, aPctHalted, aPctOther);
        updateCPUGraphsAndMetric(aPctExecuting, aPctOther);
    }

    /* Update the network load chart with values we find under /Public/NetAdapter/: */
    {
        quint64 cbNetworkTotalReceived = 0;
        quint64 cbNetworkTotalTransmitted = 0;
        UIMonitorCommon::getNetworkLoad(m_comMachineDebugger, cbNetworkTotalReceived, cbNetworkTotalTransmitted);
        updateNetworkGraphsAndMetric(cbNetworkTotalReceived, cbNetworkTotalTransmitted);
    }

    /* Update the Disk I/O chart with values we find under /Public/Storage/?/Port?/Bytes*: */
    {
        quint64 cbDiskIOTotalWritten = 0;
        quint64 cbDiskIOTotalRead = 0;
        UIMonitorCommon::getDiskLoad(m_comMachineDebugger, cbDiskIOTotalWritten, cbDiskIOTotalRead);
        updateDiskIOGraphsAndMetric(cbDiskIOTotalWritten, cbDiskIOTotalRead);
    }

    /* Update the VM exit chart with values we find as /PROF/CPU?/EM/RecordedExits: */
    {
        quint64 cTotalVMExits = 0;
        UIMonitorCommon::getVMMExitCount(m_comMachineDebugger, cTotalVMExits);
        updateVMExitMetric(cTotalVMExits);
    }
}

void UIPerformanceMonitor::sltMachineStateChange(const QUuid &uId)
{
    if (m_comMachine.isNull())
        return;
    if (m_comMachine.GetId() != uId)
        return;
    reset();
    if (m_comMachine.GetState() == KMachineState_Running)
    {
        setEnabled(true);
        openSession();
        start();
    }
}

void UIPerformanceMonitor::sltExportMetricsToFile()
{
    QString strFileName = QIFileDialog::getSaveFileName("","",this, "");

    QFile data(strFileName);
    if (data.open(QFile::WriteOnly | QFile::Truncate)) {

        for (QMap<QString, UIMetric>::const_iterator iterator =  m_metrics.begin();
             iterator != m_metrics.end(); ++iterator)
        {
            iterator.value().toFile(data);
        }
        data.close();

      //   QTextStream out(&data);
      // out << "Result: " << qSetFieldWidth(10) << left << 3.14 << 2.7;
      // writes "Result: 3.14      2.7       "
  }


}


void UIPerformanceMonitor::sltGuestAdditionsStateChange()
{
    bool fGuestAdditionsAvailable = guestAdditionsAvailable(6 /* minimum major version */);
    if (m_fGuestAdditionsAvailable == fGuestAdditionsAvailable)
        return;
    m_fGuestAdditionsAvailable = fGuestAdditionsAvailable;
    enableDisableGuestAdditionDependedWidgets(m_fGuestAdditionsAvailable);
}

void UIPerformanceMonitor::prepareMetrics()
{
    m_performanceMonitor = uiCommon().virtualBox().GetPerformanceCollector();
    if (m_performanceMonitor.isNull())
        return;

    m_nameList << "Guest/RAM/Usage*";
    m_objectList = QVector<CUnknown>(m_nameList.size(), CUnknown());
    m_performanceMonitor.SetupMetrics(m_nameList, m_objectList, g_iPeriod, g_iMetricSetupCount);
    {
        QVector<CPerformanceMetric> metrics = m_performanceMonitor.GetMetrics(m_nameList, m_objectList);
        for (int i = 0; i < metrics.size(); ++i)
        {
            QString strName(metrics[i].GetMetricName());
            if (!strName.contains(':'))
            {
                if (strName.contains("RAM", Qt::CaseInsensitive) && strName.contains("Free", Qt::CaseInsensitive))
                {
                    UIMetric newMetric(m_strRAMMetricName, metrics[i].GetUnit(), g_iMaximumQueueSize);
                    newMetric.setRequiresGuestAdditions(true);
                    m_metrics.insert(m_strRAMMetricName, newMetric);
                }
            }
        }
    }

    m_metrics.insert(m_strCPUMetricName, UIMetric(m_strCPUMetricName, "%", g_iMaximumQueueSize));
    {
        /* Network metric: */
        UIMetric networkMetric(m_strNetworkMetricName, "B", g_iMaximumQueueSize);
        m_metrics.insert(m_strNetworkMetricName, networkMetric);
    }

    /* Disk IO metric */
    {
        UIMetric diskIOMetric(m_strDiskIOMetricName, "B", g_iMaximumQueueSize);
        m_metrics.insert(m_strDiskIOMetricName, diskIOMetric);
    }

    /* VM exits metric */
    {
        UIMetric VMExitsMetric(m_strVMExitMetricName, "times", g_iMaximumQueueSize);
        m_metrics.insert(m_strVMExitMetricName, VMExitsMetric);
    }
}

void UIPerformanceMonitor::prepareToolBar()
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
    // QAction *pAction =
    //     actionPool()->action(UIActionIndex_M_Performance_S_Export);
    // if (pAction)
    //     connect(pAction, &QAction::triggered, this, &UIPerformanceMonitor::sltExportMetricsToFile);
}

bool UIPerformanceMonitor::guestAdditionsAvailable(int iMinimumMajorVersion)
{
    if (m_comGuest.isNull())
        return false;
    bool fGuestAdditionsStatus = m_comGuest.GetAdditionsStatus(m_comGuest.GetAdditionsRunLevel());
    if (fGuestAdditionsStatus)
    {
        QStringList versionStrings = m_comGuest.GetAdditionsVersion().split('.', QString::SkipEmptyParts);
        if (!versionStrings.isEmpty())
        {
            bool fConvert = false;
            int iMajorVersion = versionStrings[0].toInt(&fConvert);
            if (fConvert && iMajorVersion >= iMinimumMajorVersion)
                return true;
        }
    }
    return false;
}

void UIPerformanceMonitor::enableDisableGuestAdditionDependedWidgets(bool fEnable)
{
    for (QMap<QString, UIMetric>::const_iterator iterator =  m_metrics.begin();
         iterator != m_metrics.end(); ++iterator)
    {
        if (!iterator.value().requiresGuestAdditions())
            continue;
        if (m_charts.contains(iterator.key()) && m_charts[iterator.key()])
            m_charts[iterator.key()]->setIsAvailable(fEnable);
        if (m_infoLabels.contains(iterator.key()) && m_infoLabels[iterator.key()])
        {
            m_infoLabels[iterator.key()]->setEnabled(fEnable);
            m_infoLabels[iterator.key()]->update();
        }
    }
}

void UIPerformanceMonitor::updateCPUGraphsAndMetric(ULONG iExecutingPercentage, ULONG iOtherPercentage)
{
    UIMetric &CPUMetric = m_metrics[m_strCPUMetricName];
    CPUMetric.addData(0, iExecutingPercentage);
    CPUMetric.addData(1, iOtherPercentage);
    CPUMetric.setMaximum(100);
    if (m_infoLabels.contains(m_strCPUMetricName)  && m_infoLabels[m_strCPUMetricName])
    {
        QString strInfo;

        strInfo = QString("<b>%1</b></b><br/><font color=\"%2\">%3: %4%5</font><br/><font color=\"%6\">%7: %8%9</font>")
            .arg(m_strCPUInfoLabelTitle)
            .arg(dataColorString(m_strCPUMetricName, 0))
            .arg(m_strCPUInfoLabelGuest).arg(QString::number(iExecutingPercentage)).arg(CPUMetric.unit())
            .arg(dataColorString(m_strCPUMetricName, 1))
            .arg(m_strCPUInfoLabelVMM).arg(QString::number(iOtherPercentage)).arg(CPUMetric.unit());
        m_infoLabels[m_strCPUMetricName]->setText(strInfo);
    }

    if (m_charts.contains(m_strCPUMetricName))
        m_charts[m_strCPUMetricName]->update();
}

void UIPerformanceMonitor::updateRAMGraphsAndMetric(quint64 iTotalRAM, quint64 iFreeRAM)
{
    UIMetric &RAMMetric = m_metrics[m_strRAMMetricName];
    RAMMetric.setMaximum(iTotalRAM);
    RAMMetric.addData(0, iTotalRAM - iFreeRAM);
    if (m_infoLabels.contains(m_strRAMMetricName)  && m_infoLabels[m_strRAMMetricName])
    {
        QString strInfo;
        strInfo = QString("<b>%1</b><br/>%2: %3<br/>%4: %5<br/>%6: %7").arg(m_strRAMInfoLabelTitle).arg(m_strRAMInfoLabelTotal).arg(uiCommon().formatSize(_1K * iTotalRAM, g_iDecimalCount))
            .arg(m_strRAMInfoLabelFree).arg(uiCommon().formatSize(_1K * (iFreeRAM), g_iDecimalCount))
            .arg(m_strRAMInfoLabelUsed).arg(uiCommon().formatSize(_1K * (iTotalRAM - iFreeRAM), g_iDecimalCount));
        m_infoLabels[m_strRAMMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strRAMMetricName))
        m_charts[m_strRAMMetricName]->update();
}

void UIPerformanceMonitor::updateNetworkGraphsAndMetric(quint64 iReceiveTotal, quint64 iTransmitTotal)
{
    UIMetric &NetMetric = m_metrics[m_strNetworkMetricName];

    quint64 iReceiveRate = iReceiveTotal - NetMetric.total(0);
    quint64 iTransmitRate = iTransmitTotal - NetMetric.total(1);

    NetMetric.setTotal(0, iReceiveTotal);
    NetMetric.setTotal(1, iTransmitTotal);

    if (!NetMetric.isInitialized())
    {
        NetMetric.setIsInitialized(true);
        return;
    }

    NetMetric.addData(0, iReceiveRate);
    NetMetric.addData(1, iTransmitRate);
    quint64 iMaximum = qMax(NetMetric.maximum(), qMax(iReceiveRate, iTransmitRate));
    NetMetric.setMaximum(iMaximum);

    if (m_infoLabels.contains(m_strNetworkMetricName)  && m_infoLabels[m_strNetworkMetricName])
    {
        QString strInfo;
        strInfo = QString("<b>%1</b></b><br/><font color=\"%2\">%3: %4<br/>%5 %6</font><br/><font color=\"%7\">%8: %9<br/>%10 %11</font>")
            .arg(m_strNetworkInfoLabelTitle)
            .arg(dataColorString(m_strNetworkMetricName, 0)).arg(m_strNetworkInfoLabelReceived).arg(uiCommon().formatSize((quint64)iReceiveRate, g_iDecimalCount))
            .arg(m_strNetworkInfoLabelReceivedTotal).arg(uiCommon().formatSize((quint64)iReceiveTotal, g_iDecimalCount))
            .arg(dataColorString(m_strNetworkMetricName, 1)).arg(m_strNetworkInfoLabelTransmitted).arg(uiCommon().formatSize((quint64)iTransmitRate, g_iDecimalCount))
            .arg(m_strNetworkInfoLabelTransmittedTotal).arg(uiCommon().formatSize((quint64)iTransmitTotal, g_iDecimalCount));
        m_infoLabels[m_strNetworkMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strNetworkMetricName))
        m_charts[m_strNetworkMetricName]->update();
}

void UIPerformanceMonitor::resetCPUInfoLabel()
{
    if (m_infoLabels.contains(m_strCPUMetricName)  && m_infoLabels[m_strCPUMetricName])
    {
        QString strInfo =QString("<b>%1</b></b><br/>%2: %3<br/>%4: %5")
            .arg(m_strCPUInfoLabelTitle)
            .arg(m_strCPUInfoLabelGuest).arg("--")
            .arg(m_strCPUInfoLabelVMM).arg("--");
        m_infoLabels[m_strCPUMetricName]->setText(strInfo);
    }
}

void UIPerformanceMonitor::resetRAMInfoLabel()
{
    if (m_infoLabels.contains(m_strRAMMetricName)  && m_infoLabels[m_strRAMMetricName])
    {
        QString strInfo = QString("<b>%1</b><br/>%2: %3<br/>%4: %5<br/>%6: %7").
            arg(m_strRAMInfoLabelTitle).arg(m_strRAMInfoLabelTotal).arg("--")
            .arg(m_strRAMInfoLabelFree).arg("--")
            .arg(m_strRAMInfoLabelUsed).arg("--");
        m_infoLabels[m_strRAMMetricName]->setText(strInfo);
    }
}

void UIPerformanceMonitor::resetNetworkInfoLabel()
{
    if (m_infoLabels.contains(m_strNetworkMetricName)  && m_infoLabels[m_strNetworkMetricName])
    {
        QString strInfo = QString("<b>%1</b></b><br/>%2: %3<br/>%4 %5<br/>%6: %7<br/>%8 %9")
            .arg(m_strNetworkInfoLabelTitle)
            .arg(m_strNetworkInfoLabelReceived).arg("--")
            .arg(m_strNetworkInfoLabelReceivedTotal).arg("--")
            .arg(m_strNetworkInfoLabelTransmitted).arg("--")
            .arg(m_strNetworkInfoLabelTransmittedTotal).arg("--");
        m_infoLabels[m_strNetworkMetricName]->setText(strInfo);
    }
}

void UIPerformanceMonitor::resetVMExitInfoLabel()
{
    if (m_infoLabels.contains(m_strVMExitMetricName)  && m_infoLabels[m_strVMExitMetricName])
    {
        QString strInfo;
        strInfo = QString("<b>%1</b></b><br/>%2: %3<br/>%4: %5")
            .arg(m_strVMExitInfoLabelTitle)
            .arg(m_strVMExitLabelCurrent).arg("--")
            .arg(m_strVMExitLabelTotal).arg("--");

        m_infoLabels[m_strVMExitMetricName]->setText(strInfo);
    }
}

void UIPerformanceMonitor::resetDiskIOInfoLabel()
{
    if (m_infoLabels.contains(m_strDiskIOMetricName)  && m_infoLabels[m_strDiskIOMetricName])
    {
        QString strInfo = QString("<b>%1</b></b><br/>%2: %3<br/>%4 %5<br/>%6: %7<br/>%8 %9")
            .arg(m_strDiskIOInfoLabelTitle)
            .arg(m_strDiskIOInfoLabelWritten).arg("--")
            .arg(m_strDiskIOInfoLabelWrittenTotal).arg("--")
            .arg(m_strDiskIOInfoLabelRead).arg("--")
            .arg(m_strDiskIOInfoLabelReadTotal).arg("--");
        m_infoLabels[m_strDiskIOMetricName]->setText(strInfo);
    }
}

void UIPerformanceMonitor::updateDiskIOGraphsAndMetric(quint64 uDiskIOTotalWritten, quint64 uDiskIOTotalRead)
{
    UIMetric &diskMetric = m_metrics[m_strDiskIOMetricName];

    quint64 iWriteRate = uDiskIOTotalWritten - diskMetric.total(0);
    quint64 iReadRate = uDiskIOTotalRead - diskMetric.total(1);

    diskMetric.setTotal(0, uDiskIOTotalWritten);
    diskMetric.setTotal(1, uDiskIOTotalRead);

    diskMetric.addData(0, iWriteRate);
    diskMetric.addData(1, iReadRate);
    /* Do not set data and maximum if the metric has not been initialized  since we need to initialize totals "(t-1)" first: */
    if (!diskMetric.isInitialized()){
        diskMetric.setIsInitialized(true);
        return;
    }

    quint64 iMaximum = qMax(diskMetric.maximum(), qMax(iWriteRate, iReadRate));
    diskMetric.setMaximum(iMaximum);

    if (m_infoLabels.contains(m_strDiskIOMetricName)  && m_infoLabels[m_strDiskIOMetricName])
    {
        QString strInfo = QString("<b>%1</b></b><br/><font color=\"%2\">%3: %4<br/>%5 %6</font><br/><font color=\"%7\">%8: %9<br/>%10 %11</font>")
            .arg(m_strDiskIOInfoLabelTitle)
            .arg(dataColorString(m_strDiskIOMetricName, 0)).arg(m_strDiskIOInfoLabelWritten).arg(uiCommon().formatSize((quint64)iWriteRate, g_iDecimalCount))
            .arg(m_strDiskIOInfoLabelWrittenTotal).arg(uiCommon().formatSize((quint64)uDiskIOTotalWritten, g_iDecimalCount))
            .arg(dataColorString(m_strDiskIOMetricName, 1)).arg(m_strDiskIOInfoLabelRead).arg(uiCommon().formatSize((quint64)iReadRate, g_iDecimalCount))
            .arg(m_strDiskIOInfoLabelReadTotal).arg(uiCommon().formatSize((quint64)uDiskIOTotalRead, g_iDecimalCount));
        m_infoLabels[m_strDiskIOMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strDiskIOMetricName))
        m_charts[m_strDiskIOMetricName]->update();
}

void UIPerformanceMonitor::updateVMExitMetric(quint64 uTotalVMExits)
{
    if (uTotalVMExits <= 0)
        return;

    UIMetric &VMExitMetric = m_metrics[m_strVMExitMetricName];
    quint64 iRate = uTotalVMExits - VMExitMetric.total(0);
    VMExitMetric.setTotal(0, uTotalVMExits);
    /* Do not set data and maximum if the metric has not been initialized  since we need to initialize totals "(t-1)" first: */
    if (!VMExitMetric.isInitialized())
    {
        VMExitMetric.setIsInitialized(true);
        return;
    }
    VMExitMetric.addData(0, iRate);
    quint64 iMaximum = qMax(VMExitMetric.maximum(), iRate);
    VMExitMetric.setMaximum(iMaximum);
    if (m_infoLabels.contains(m_strVMExitMetricName)  && m_infoLabels[m_strVMExitMetricName])
    {
        QString strInfo;
        strInfo = QString("<b>%1</b></b><br/>%2: %3 %4<br/>%5: %6 %7")
            .arg(m_strVMExitInfoLabelTitle)
            .arg(m_strVMExitLabelCurrent).arg(UICommon::addMetricSuffixToNumber(iRate)).arg(VMExitMetric.unit())
            .arg(m_strVMExitLabelTotal).arg(UICommon::addMetricSuffixToNumber(uTotalVMExits)).arg(VMExitMetric.unit());
         m_infoLabels[m_strVMExitMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strVMExitMetricName))
        m_charts[m_strVMExitMetricName]->update();
}

QString UIPerformanceMonitor::dataColorString(const QString &strChartName, int iDataIndex)
{
    if (!m_charts.contains(strChartName))
        return QColor(Qt::black).name(QColor::HexRgb);
    UIChart *pChart = m_charts[strChartName];
    if (!pChart)
        return QColor(Qt::black).name(QColor::HexRgb);
    return pChart->dataSeriesColor(iDataIndex).name(QColor::HexRgb);
}

void UIPerformanceMonitor::reset()
{
    m_fGuestAdditionsAvailable = false;
    setEnabled(false);

    if (m_pTimer)
        m_pTimer->stop();
    /* reset the metrics. this will delete their data cache: */
    for (QMap<QString, UIMetric>::iterator iterator =  m_metrics.begin();
         iterator != m_metrics.end(); ++iterator)
        iterator.value().reset();
    /* force update on the charts to draw now emptied metrics' data: */
    for (QMap<QString, UIChart*>::iterator iterator =  m_charts.begin();
         iterator != m_charts.end(); ++iterator)
        iterator.value()->update();
    /* Reset the info labels: */
    resetCPUInfoLabel();
    resetRAMInfoLabel();
    resetNetworkInfoLabel();
    resetDiskIOInfoLabel();
    resetVMExitInfoLabel();
    update();
}

void UIPerformanceMonitor::start()
{
    if (m_comMachine.isNull() || m_comMachine.GetState() != KMachineState_Running)
        return;

    m_fGuestAdditionsAvailable = guestAdditionsAvailable(6 /* minimum major version */);
    enableDisableGuestAdditionDependedWidgets(m_fGuestAdditionsAvailable);
    if (m_pTimer)
        m_pTimer->start(1000 * g_iPeriod);
}


#include "UIPerformanceMonitor.moc"
