#include "mainwindow.h"
#include "qcustomplot.h"
#include <QHeaderView>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , udpSocket(new QUdpSocket(this))
    , updateTimer(new QTimer(this))
    , tabWidget(new QTabWidget(this))
    , totalLabel(new QLabel("Total: —"))
    , coresTable(new QTableWidget(0, 2, this))
    , customPlot(new QCustomPlot(this))
    , mainLayout(nullptr)
    , totalGraph(nullptr)
    , cpuTag(nullptr)  // Инициализация индикатора
    , currentTimeSec(QDateTime::currentSecsSinceEpoch())
{
    setupUI();

    // Настройка таймера для обновления оси X (каждую секунду)
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateXAxisRange);
    updateTimer->start(1000);

    // Открываем сокет
    if (!udpSocket->bind(QHostAddress::LocalHost, 1234)) {
        totalLabel->setText(QString("Bind error: %1").arg(udpSocket->errorString()));
        return;
    }
    connect(udpSocket, &QUdpSocket::readyRead, this, &MainWindow::onReadyRead);
}

MainWindow::~MainWindow()
{
    // Удаляем индикатор, если он был создан
    if (cpuTag) {
        delete cpuTag;
    }
}

double MainWindow::calculateTotalCpuUsage(const QVector<double> &cpuUsages)
{
    if (cpuUsages.isEmpty()) return 0.0;
    double sum = 0.0;
    for (double usage : cpuUsages) sum += usage;
    return sum / cpuUsages.size();
}

double MainWindow::roundToTen(double value)
{
    return ceil(value / 10.0) * 10.0;
}

void MainWindow::setupUI()
{
    // === Вкладка 1: Таблица ===
    totalLabel->setStyleSheet("font-size: 16pt; font-weight: bold; padding: 8px;");
    coresTable->setHorizontalHeaderLabels({"Core", "Usage"});
    coresTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    coresTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    coresTable->setEditTriggers(QTableWidget::NoEditTriggers);
    coresTable->setFocusPolicy(Qt::NoFocus);
    coresTable->setSelectionMode(QAbstractItemView::NoSelection);
    coresTable->verticalHeader()->setVisible(false);

    QWidget *tableTab = new QWidget(this);
    QVBoxLayout *tableLayout = new QVBoxLayout(tableTab);
    tableLayout->addWidget(totalLabel);
    tableLayout->addWidget(coresTable);
    tableLayout->setContentsMargins(10, 10, 10, 10);

    // === Вкладка 2: Графики ===
    QWidget *plotTab = new QWidget(this);
    QVBoxLayout *plotTabLayout = new QVBoxLayout(plotTab);
    plotTabLayout->setContentsMargins(0, 0, 0, 0);
    plotTabLayout->addWidget(customPlot);

    // Настройка графика
    customPlot->xAxis->setLabel("Time");
    customPlot->yAxis->setLabel("CPU Usage (%)");

    // === ДОБАВЛЯЕМ ПРАВУЮ ОСЬ Y ===
    customPlot->yAxis2->setVisible(true);
    customPlot->yAxis2->setLabel("Total CPU");
    customPlot->yAxis2->setTickLabels(true);

    // Связываем диапазоны осей Y (левой и правой)
    connect(customPlot->yAxis, SIGNAL(rangeChanged(QCPRange)),
            customPlot->yAxis2, SLOT(setRange(QCPRange)));

    // Добавляем отступ для индикатора
    customPlot->yAxis2->setPadding(30);

    // Настройка оси X для отображения времени
    QSharedPointer<QCPAxisTickerDateTime> dateTimeTicker(new QCPAxisTickerDateTime);
    dateTimeTicker->setDateTimeFormat("HH.mm");
    dateTimeTicker->setTickStepStrategy(QCPAxisTicker::tssMeetTickCount);
    dateTimeTicker->setTickCount(6);
    customPlot->xAxis->setTicker(dateTimeTicker);

    // Инициализация диапазона оси X (последние 5 минут)
    currentTimeSec = QDateTime::currentSecsSinceEpoch();
    customPlot->xAxis->setRange(currentTimeSec - X_VISIBLE_MINUTES * 60, currentTimeSec);

    // Начальный диапазон оси Y
    customPlot->yAxis->setRange(0, 100);

    // === УБИРАЕМ ЛЕГЕНДУ ===
    customPlot->legend->setVisible(false);

    // График общей нагрузки
    totalGraph = customPlot->addGraph(customPlot->xAxis, customPlot->yAxis);
    totalGraph->setPen(QPen(QColor(0, 0, 0), 4));
    totalGraph->setVisible(true);

    // === СОЗДАЕМ ИНДИКАТОР ДЛЯ ПРАВОЙ ОСИ ===
    cpuTag = new AxisTag(customPlot->yAxis2);
    cpuTag->setPen(QPen(QColor(0, 0, 0), 2));
    cpuTag->setBrush(QBrush(Qt::white));

    // Стиль оформления
    customPlot->setBackground(QColor(240, 240, 240));
    customPlot->xAxis->grid()->setPen(QPen(QColor(180, 180, 180), 1));
    customPlot->yAxis->grid()->setPen(QPen(QColor(180, 180, 180), 1));
    customPlot->xAxis->grid()->setVisible(true);
    customPlot->yAxis->grid()->setVisible(true);
    customPlot->yAxis2->grid()->setVisible(false); // Сетка только для левой оси

    // === Объединение вкладок ===
    tabWidget->addTab(tableTab, "CPU Table");
    tabWidget->addTab(plotTab, "QCustomPlot");
    setCentralWidget(tabWidget);
    setWindowTitle("CPU Monitor (UDP: localhost:1234)");
    resize(900, 600);
}

QColor MainWindow::getColorForCore(int coreIndex)
{
    if (coreIndex < coreColors.size()) return coreColors[coreIndex];
    int hue = (coreIndex * 41) % 360;
    return QColor::fromHsv(hue, 200, 255);
}

void MainWindow::updateXAxisRange()
{
    currentTimeSec = QDateTime::currentSecsSinceEpoch();
    customPlot->xAxis->setRange(currentTimeSec - X_VISIBLE_MINUTES * 60, currentTimeSec);

    if (!timeHistory.isEmpty()) {
        // Обновляем ключи для существующих данных
        for (int i = 0; i < cpuGraphs.size(); ++i) {
            if (i < cpuHistory.size() && !cpuHistory[i].isEmpty()) {
                QVector<double> keys;
                int startIdx = qMax(0, timeHistory.size() - cpuHistory[i].size());
                for (int j = startIdx; j < timeHistory.size(); ++j) {
                    keys.append(timeHistory[j]);
                }
                cpuGraphs[i]->setData(keys, cpuHistory[i]);
            }
        }

        // Обновляем график общей нагрузки
        if (!totalCpuHistory.isEmpty()) {
            QVector<double> totalKeys;
            int startIdx = qMax(0, timeHistory.size() - totalCpuHistory.size());
            for (int j = startIdx; j < timeHistory.size(); ++j) {
                totalKeys.append(timeHistory[j]);
            }
            totalGraph->setData(totalKeys, totalCpuHistory);

            // === ОБНОВЛЯЕМ ИНДИКАТОР ===
            double lastValue = totalCpuHistory.last();
            cpuTag->updatePosition(lastValue);
            cpuTag->setText(QString::number(lastValue, 'f', 1) + " %");
        }

        updateYAxisRange();
        customPlot->replot();
    }
}

void MainWindow::updateYAxisRange()
{
    double yMax = 0.0;
    double minVisibleTime = currentTimeSec - X_VISIBLE_MINUTES * 60;

    // Проверяем данные ядер
    for (int i = 0; i < cpuHistory.size(); ++i) {
        for (int j = 0; j < cpuHistory[i].size(); ++j) {
            if (timeHistory.size() - cpuHistory[i].size() + j >= 0) {
                double dataTime = timeHistory[timeHistory.size() - cpuHistory[i].size() + j];
                if (dataTime >= minVisibleTime) {
                    yMax = qMax(yMax, cpuHistory[i][j]);
                }
            }
        }
    }

    // Проверяем общую нагрузку
    for (int j = 0; j < totalCpuHistory.size(); ++j) {
        if (timeHistory.size() - totalCpuHistory.size() + j >= 0) {
            double dataTime = timeHistory[timeHistory.size() - totalCpuHistory.size() + j];
            if (dataTime >= minVisibleTime) {
                yMax = qMax(yMax, totalCpuHistory[j]);
            }
        }
    }

    if (yMax < 1e-6) yMax = 0.0;

    double yMaxWithMargin = roundToTen(yMax * 1.1);
    if (yMaxWithMargin < 10) yMaxWithMargin = 10;

    customPlot->yAxis->setRange(0, yMaxWithMargin);

    double tickStep = yMaxWithMargin * 0.05;
    if (tickStep < 1) tickStep = 1;

    QSharedPointer<QCPAxisTickerFixed> ticker(new QCPAxisTickerFixed);
    ticker->setTickStep(tickStep);
    ticker->setScaleStrategy(QCPAxisTickerFixed::ssMultiples);
    customPlot->yAxis->setTicker(ticker);
}

void MainWindow::onReadyRead()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(udpSocket->pendingDatagramSize()));
        udpSocket->readDatagram(datagram.data(), datagram.size());
        parseAndDisplay(datagram);
    }
}

void MainWindow::parseAndDisplay(const QByteArray &data)
{
    QString text = QString::fromUtf8(data).trimmed();
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    if (!lines.isEmpty() && lines[0].startsWith("Total:")) {
        totalLabel->setText(lines[0].trimmed());
    }

    int coreCount = lines.size() - 1;
    if (coreCount <= 0) return;

    // Инициализация при первом получении данных
    if (coresTable->rowCount() == 0) {
        coresTable->setRowCount(coreCount);
        for (int i = 0; i < coreCount; ++i) {
            QTableWidgetItem *coreItem = new QTableWidgetItem(QString("Core %1").arg(i));
            coreItem->setTextAlignment(Qt::AlignCenter);
            coresTable->setItem(i, 0, coreItem);

            QProgressBar *bar = new QProgressBar();
            bar->setRange(0, 100);
            bar->setTextVisible(true);
            bar->setFormat("%v%");
            coresTable->setCellWidget(i, 1, bar);
        }

        cpuHistory.resize(coreCount);
        for (int i = 0; i < coreCount; ++i) {
            cpuHistory[i].reserve(MAX_HISTORY_POINTS);
            QCPGraph *graph = customPlot->addGraph(customPlot->xAxis, customPlot->yAxis);
            QColor color = getColorForCore(i);
            graph->setPen(QPen(color, 1));
            graph->setVisible(true);
            cpuGraphs.append(graph);
        }

        totalCpuHistory.reserve(MAX_HISTORY_POINTS);
        timeHistory.reserve(MAX_HISTORY_POINTS);
    }

    QVector<double> currentUsages(coreCount, 0.0);

    for (int i = 1; i < lines.size() && (i - 1) < coresTable->rowCount(); ++i) {
        const QString &line = lines[i].trimmed();
        QRegularExpression re(R"(Core (\d+): ([\d.]+)%)");
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            int coreIdx = match.captured(1).toInt();
            qreal usage = match.captured(2).toDouble();
            currentUsages[coreIdx] = usage;

            if (QProgressBar *bar = qobject_cast<QProgressBar*>(coresTable->cellWidget(coreIdx, 1))) {
                bar->setValue(static_cast<int>(usage));
                QString color = usage > 80 ? "#ff4444" : (usage > 50 ? "#ffaa00" : "#44ff44");
                bar->setStyleSheet(QString("QProgressBar::chunk { background-color: %1; }").arg(color));
            }
        }
    }

    updatePlots(currentUsages);
}

void MainWindow::updatePlots(const QVector<double> &cpuUsages)
{
    int coreCount = cpuUsages.size();
    if (coreCount == 0 || cpuGraphs.size() != coreCount) return;

    double totalUsage = calculateTotalCpuUsage(cpuUsages);
    currentTimeSec = QDateTime::currentSecsSinceEpoch();

    // Добавляем текущее время в историю
    timeHistory.append(currentTimeSec);
    if (timeHistory.size() > MAX_HISTORY_POINTS) {
        timeHistory.remove(0);
    }

    // Обновляем данные ядер
    for (int i = 0; i < coreCount; ++i) {
        cpuHistory[i].append(cpuUsages[i]);
        if (cpuHistory[i].size() > MAX_HISTORY_POINTS) {
            cpuHistory[i].remove(0);
        }

        QVector<double> keys;
        int startIdx = qMax(0, timeHistory.size() - cpuHistory[i].size());
        for (int j = startIdx; j < timeHistory.size(); ++j) {
            keys.append(timeHistory[j]);
        }
        cpuGraphs[i]->setData(keys, cpuHistory[i]);
    }

    // Обновляем общую нагрузку
    totalCpuHistory.append(totalUsage);
    if (totalCpuHistory.size() > MAX_HISTORY_POINTS) {
        totalCpuHistory.remove(0);
    }

    QVector<double> totalKeys;
    int startIdx = qMax(0, timeHistory.size() - totalCpuHistory.size());
    for (int j = startIdx; j < timeHistory.size(); ++j) {
        totalKeys.append(timeHistory[j]);
    }
    totalGraph->setData(totalKeys, totalCpuHistory);

    // === ОБНОВЛЯЕМ ИНДИКАТОР ===
    cpuTag->updatePosition(totalUsage);
    cpuTag->setText(QString::number(totalUsage, 'f', 1) + " %");

    // Обновляем диапазон оси X
    customPlot->xAxis->setRange(currentTimeSec - X_VISIBLE_MINUTES * 60, currentTimeSec);

    updateYAxisRange();
    customPlot->replot();
}
