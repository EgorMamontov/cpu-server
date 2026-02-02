#include "mainwindow.h"
#include "qcustomplot.h"
#include <QHeaderView>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QLoggingCategory>
#include <numeric>

// Категория логирования для отладки
Q_LOGGING_CATEGORY(cpuMonitor, "app.cpumonitor")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , udpSocket(new QUdpSocket(this))
    , updateTimer(new QTimer(this))
    , tabWidget(new QTabWidget(this))
    , totalLabel(new QLabel("Total: —"))
    , coresTable(new QTableWidget(0, 2, this))
    , customPlot(new QCustomPlot(this))
    , totalGraph(nullptr)
    , totalCpuIndicator(nullptr)
    , currentTimeSec(QDateTime::currentSecsSinceEpoch())
{
    setupUI();

    // Настройка таймера для обновления оси X (каждую секунду)
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateXAxisRange);
    updateTimer->start(1000);

    // Открываем сокет с проверкой ошибок
    if (!udpSocket->bind(QHostAddress::LocalHost, 1234)) {
        totalLabel->setText(QString("Bind error: %1").arg(udpSocket->errorString()));
        qCCritical(cpuMonitor) << "Failed to bind UDP socket:" << udpSocket->errorString();
        return;
    }
    connect(udpSocket, &QUdpSocket::readyRead, this, &MainWindow::onReadyRead);
}

MainWindow::~MainWindow()
{
    // Удаляем индикатор, если он был создан
    delete totalCpuIndicator;
}

double MainWindow::calculateTotalCpuUsage(const QVector<double> &cpuUsages)
{
    if (cpuUsages.isEmpty()) return 0.0;

    // Используем STL алгоритм для лучшей читабельности
    double sum = std::accumulate(cpuUsages.begin(), cpuUsages.end(), 0.0);
    return sum / cpuUsages.size();
}

double MainWindow::roundToTen(double value)
{
    // проверяем отрицательные значения
    if (value < 0.0) {
        qCWarning(cpuMonitor) << "Negative value passed to roundToTen:" << value;
        return 0.0;
    }

    // Округление до ближайшего большего десятка
    return ceil(value / 10.0) * 10.0;
}

QVector<QColor> MainWindow::getDefaultCoreColors() const
{
    // Выносим цвета в отдельный метод для лучшей организации
    return QVector<QColor>{
        QColor(255, 0, 0),     // Красный
        QColor(0, 180, 60),    // Зеленый
        QColor(0, 0, 255),     // Синий
        QColor(255, 165, 0),   // Оранжевый
        QColor(128, 0, 128),   // Фиолетовый
        QColor(0, 255, 255),   // Голубой
        QColor(255, 0, 255),   // Розовый
        QColor(139, 69, 19),   // Коричневый
        QColor(255, 192, 203), // Светло-розовый
        QColor(128, 128, 128), // Серый
        QColor(0, 128, 128),   // Бирюзовый
        QColor(128, 0, 0),     // Темно-красный
        QColor(75, 0, 130),    // Индиго
        QColor(255, 215, 0),   // Золотой
        QColor(64, 224, 208),  // Бирюзовый2
        QColor(255, 105, 180)  // Розовый2
    };
}

QColor MainWindow::getColorForCore(int coreIndex)
{
    QVector<QColor> defaultColors = getDefaultCoreColors();

    if (coreIndex < defaultColors.size()) {
        return defaultColors[coreIndex];
    }

    // Если ядер больше, чем предопределенных цветов, генерируем новый цвет
    int hue = (coreIndex * 41) % 360; // 41 - простое число для лучшего распределения
    return QColor::fromHsv(hue, 200, 255);
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

    // Добавляем отступ для индикатора (используем именованную константу)
    customPlot->yAxis2->setPadding(Y_AXIS_PADDING_FOR_TAG);

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
    totalCpuIndicator = new AxisTag(customPlot->yAxis2);
    totalCpuIndicator->setPen(QPen(QColor(0, 0, 0), 2));
    totalCpuIndicator->setBrush(QBrush(Qt::white));

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

void MainWindow::updateXAxisRange()
{
    currentTimeSec = QDateTime::currentSecsSinceEpoch();
    customPlot->xAxis->setRange(currentTimeSec - X_VISIBLE_MINUTES * 60, currentTimeSec);

    if (!timeHistory.isEmpty()) {
        // Обновляем ключи для существующих данных
        for (int i = 0; i < cpuGraphs.size(); ++i) {
            // проверяем границы
            if (i >= cpuHistory.size() || cpuHistory[i].isEmpty()) {
                continue;
            }

            QVector<double> keys;
            int startIdx = qMax(0, timeHistory.size() - cpuHistory[i].size());
            for (int j = startIdx; j < timeHistory.size(); ++j) {
                keys.append(timeHistory[j]);
            }
            cpuGraphs[i]->setData(keys, cpuHistory[i]);
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
            totalCpuIndicator->updatePosition(lastValue);
            totalCpuIndicator->setText(QString::number(lastValue, 'f', 1) + " %");
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
            int timeIndex = timeHistory.size() - cpuHistory[i].size() + j;
            // Безопасность: проверяем корректность индекса
            if (timeIndex >= 0 && timeIndex < timeHistory.size()) {
                double dataTime = timeHistory[timeIndex];
                if (dataTime >= minVisibleTime) {
                    yMax = qMax(yMax, cpuHistory[i][j]);
                }
            }
        }
    }

    // Проверяем общую нагрузку
    for (int j = 0; j < totalCpuHistory.size(); ++j) {
        int timeIndex = timeHistory.size() - totalCpuHistory.size() + j;
        if (timeIndex >= 0 && timeIndex < timeHistory.size()) {
            double dataTime = timeHistory[timeIndex];
            if (dataTime >= minVisibleTime) {
                yMax = qMax(yMax, totalCpuHistory[j]);
            }
        }
    }

    if (yMax < 1e-6) yMax = 0.0;

    // Расчет максимального значения оси Y с запасом 10% и округлением до десятков
    // 1. Увеличиваем максимальное значение на 10% для лучшей визуализации
    // 2. Округляем до ближайшего большего десятка
    // 3. Гарантируем минимальный диапазон для отображения
    double yMaxWithMargin = roundToTen(yMax * Y_AXIS_MARGIN_FACTOR);
    if (yMaxWithMargin < MIN_Y_AXIS_RANGE) {
        yMaxWithMargin = MIN_Y_AXIS_RANGE;
    }

    customPlot->yAxis->setRange(0, yMaxWithMargin);

    // Расчет шага делений оси Y (5% от диапазона)
    // Это обеспечивает примерно 20 делений на оси, что удобно для восприятия
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
        // проверяем размер датаграммы
        qint64 pendingSize = udpSocket->pendingDatagramSize();
        if (pendingSize <= 0 || pendingSize > MAX_UDP_DATAGRAM_SIZE) {
            qCWarning(cpuMonitor) << "Invalid datagram size:" << pendingSize;
            udpSocket->readDatagram(nullptr, 0); // Сбрасываем пакет
            continue;
        }

        QByteArray datagram;
        datagram.resize(static_cast<int>(pendingSize));

        qint64 bytesRead = udpSocket->readDatagram(datagram.data(), datagram.size());
        if (bytesRead != pendingSize) {
            qCWarning(cpuMonitor) << "Incomplete datagram read:" << bytesRead << "of" << pendingSize;
            continue;
        }

        parseAndDisplay(datagram);
    }
}

void MainWindow::parseAndDisplay(const QByteArray &data)
{
    QString text = QString::fromUtf8(data).trimmed();
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    // базовая проверка формата данных
    if (lines.isEmpty() || !lines[0].startsWith("Total:")) {
        qCWarning(cpuMonitor) << "Invalid data format received";
        return;
    }

    totalLabel->setText(lines[0].trimmed());

    int coreCount = lines.size() - 1;
    if (coreCount <= 0) {
        qCDebug(cpuMonitor) << "No core data received";
        return;
    }

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

            // проверяем индекс ядра
            if (coreIdx < 0 || coreIdx >= coreCount) {
                qCWarning(cpuMonitor) << "Invalid core index:" << coreIdx;
                continue;
            }

            qreal usage = match.captured(2).toDouble();
            currentUsages[coreIdx] = usage;

            QWidget *widget = coresTable->cellWidget(coreIdx, 1);
            if (!widget) {
                qCWarning(cpuMonitor) << "No progress bar widget for core" << coreIdx;
                continue;
            }

            if (QProgressBar *bar = qobject_cast<QProgressBar*>(widget)) {
                bar->setValue(static_cast<int>(usage));
                QString color = usage > 80 ? "#ff4444" : (usage > 50 ? "#ffaa00" : "#44ff44");
                bar->setStyleSheet(QString("QProgressBar::chunk { background-color: %1; }").arg(color));
            }
        } else {
            qCDebug(cpuMonitor) << "Failed to parse line:" << line;
        }
    }

    updatePlots(currentUsages);
}

void MainWindow::updatePlots(const QVector<double> &cpuUsages)
{
    int coreCount = cpuUsages.size();
    if (coreCount == 0 || cpuGraphs.size() != coreCount) {
        qCWarning(cpuMonitor) << "Core count mismatch:" << coreCount << "vs" << cpuGraphs.size();
        return;
    }

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
    totalCpuIndicator->updatePosition(totalUsage);
    totalCpuIndicator->setText(QString::number(totalUsage, 'f', 1) + " %");

    // Обновляем диапазон оси X
    customPlot->xAxis->setRange(currentTimeSec - X_VISIBLE_MINUTES * 60, currentTimeSec);

    updateYAxisRange();
    customPlot->replot();
}
