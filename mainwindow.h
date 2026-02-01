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
#include "axistag.h"  // Добавлен заголовочный файл для AxisTag

class QCustomPlot;
class QCPGraph;
class QCPLayoutGrid;

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

    QUdpSocket *udpSocket;
    QTimer *updateTimer;

    QTabWidget *tabWidget;
    QLabel *totalLabel;
    QTableWidget *coresTable;

    QCustomPlot *customPlot;
    QCPLayoutGrid *mainLayout;

    QVector<QCPGraph*> cpuGraphs;
    QCPGraph *totalGraph;
    AxisTag *cpuTag;  // Индикатор для общей CPU загрузки

    QVector<QVector<double>> cpuHistory;
    QVector<double> totalCpuHistory;
    QVector<double> timeHistory;

    static const int MAX_HISTORY_POINTS = 300;
    double currentTimeSec;

    static const int X_VISIBLE_MINUTES = 5;
    static const int X_TICK_STEP_SECONDS = 60;

    QVector<QColor> coreColors = {
        QColor(255, 0, 0),
        QColor(0, 180, 60),
        QColor(0, 0, 255),
        QColor(255, 165, 0),
        QColor(128, 0, 128),
        QColor(0, 255, 255),
        QColor(255, 0, 255),
        QColor(139, 69, 19),
        QColor(255, 192, 203),
        QColor(128, 128, 128),
        QColor(0, 128, 128),
        QColor(128, 0, 0),
        QColor(75, 0, 130),
        QColor(255, 215, 0),
        QColor(64, 224, 208),
        QColor(255, 105, 180)
    };
};

#endif // MAINWINDOW_H
