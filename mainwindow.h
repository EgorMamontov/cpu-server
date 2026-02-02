#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include <QLabel>
#include <QTableWidget>
#include <QProgressBar>
#include <QTabWidget>
#include <QVector>
#include <QColor>
#include <QTimer>
#include "axistag.h"

class QCustomPlot;
class QCPGraph;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onReadyRead();
    void updateXAxisRange();

private:
    void setupUI();
    void parseAndDisplay(const QByteArray &data);
    void updatePlots(const QVector<double> &cpuUsages);
    void updateYAxisRange();
    QColor getColorForCore(int coreIndex);
    double calculateTotalCpuUsage(const QVector<double> &cpuUsages);
    double roundToTen(double value);

    static constexpr qint64 MAX_UDP_DATAGRAM_SIZE = 4096;
    static constexpr int MAX_HISTORY_POINTS = 300;
    static constexpr int X_VISIBLE_MINUTES = 5;
    static constexpr int Y_AXIS_PADDING_FOR_TAG = 30;
    static constexpr double Y_AXIS_MARGIN_FACTOR = 1.1;
    static constexpr double MIN_Y_AXIS_RANGE = 10.0;

    QUdpSocket *udpSocket;
    QTimer *updateTimer;

    QTabWidget *tabWidget;
    QLabel *totalLabel;
    QTableWidget *coresTable;

    QCustomPlot *customPlot;
    QVector<QCPGraph*> cpuGraphs;
    QCPGraph *totalGraph;
    AxisTag *totalCpuIndicator;

    QVector<QVector<double>> cpuHistory;
    QVector<double> totalCpuHistory;
    QVector<double> timeHistory;

    double currentTimeSec;

    // Выносим цвета по умолчанию в приватный метод
    QVector<QColor> getDefaultCoreColors() const;
};

#endif // MAINWINDOW_H
