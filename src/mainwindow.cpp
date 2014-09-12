#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QShortcut>

#include "configdialog.h"
#include "dataview.h"

MainWindow::MainWindow(
        QWidget *parent):

    QMainWindow(parent),
    m_ui(new Ui::MainWindow),
    m_xAxis(Time),
    mMarkActive(false),
    m_viewDataRotation(0),
    m_units(PlotValue::Imperial)
{
    m_ui->setupUi(this);

    // Ensure that closeEvent is called
    QObject::connect(m_ui->actionExit, SIGNAL(triggered()),
                     this, SLOT(close()));

    // Intitialize plot area
    initPlot();

    // Initialize 3D views
    initViews();

    // Restore window state
    readSettings();

    // Set default tool
    setTool(Pan);

#ifdef Q_OS_MAC
    // Fix for single-key shortcuts on Mac
    // http://thebreakfastpost.com/2014/06/03/single-key-menu-shortcuts-with-qt5-on-osx/
    foreach (QAction *a, m_ui->menuPlots->actions())
    {
        QObject::connect(new QShortcut(a->shortcut(), a->parentWidget()),
                         SIGNAL(activated()), a, SLOT(trigger()));
    }
    foreach (QAction *a, m_ui->menu_Tools->actions())
    {
        QObject::connect(new QShortcut(a->shortcut(), a->parentWidget()),
                         SIGNAL(activated()), a, SLOT(trigger()));
    }
#endif
}

MainWindow::~MainWindow()
{
    while (!m_xValues.isEmpty())
    {
        delete m_xValues.takeLast();
    }

    while (!m_yValues.isEmpty())
    {
        delete m_yValues.takeLast();
    }

    delete m_ui;
}

void MainWindow::writeSettings()
{
    QSettings settings("FlySight", "Viewer");

    settings.beginGroup("mainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("state", saveState());
    settings.setValue("units", m_units);
    settings.setValue("xAxis", m_xAxis);
    settings.endGroup();
}

void MainWindow::readSettings()
{
    QSettings settings("FlySight", "Viewer");

    settings.beginGroup("mainWindow");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("state").toByteArray());
    m_units = (PlotValue::Units) settings.value("units", m_units).toInt();
    m_xAxis = (XAxisType) settings.value("xAxis", m_xAxis).toInt();
    settings.endGroup();
}

void MainWindow::initPlot()
{
    m_xValues.append(new PlotTime);
    m_xValues.append(new PlotDistance2D);
    m_xValues.append(new PlotDistance3D);

    foreach (PlotValue *v, m_xValues)
    {
        v->readSettings();
    }

    updateBottomActions();

    m_yValues.append(new PlotElevation);
    m_yValues.append(new PlotVerticalSpeed);
    m_yValues.append(new PlotHorizontalSpeed);
    m_yValues.append(new PlotTotalSpeed);
    m_yValues.append(new PlotDiveAngle);
    m_yValues.append(new PlotCurvature);
    m_yValues.append(new PlotGlideRatio);
    m_yValues.append(new PlotHorizontalAccuracy);
    m_yValues.append(new PlotVerticalAccuracy);
    m_yValues.append(new PlotSpeedAccuracy);
    m_yValues.append(new PlotNumberOfSatellites);

    foreach (PlotValue *v, m_yValues)
    {
        v->readSettings();
    }

    updateLeftActions();

    m_ui->plotArea->setMainWindow(this);

    connect(m_ui->plotArea, SIGNAL(zero(double)),
            this, SLOT(onDataPlot_zero(double)));
    connect(m_ui->plotArea, SIGNAL(ground(double)),
            this, SLOT(onDataPlot_ground(double)));

    connect(this, SIGNAL(rangeChanged(double, double)),
            m_ui->plotArea, SLOT(setRange(double, double)));
    connect(this, SIGNAL(dataChanged()),
            m_ui->plotArea, SLOT(updatePlot()));
    connect(this, SIGNAL(plotChanged()),
            m_ui->plotArea, SLOT(updatePlot()));
}

void MainWindow::initViews()
{
    initSingleView(tr("Top View"), "topView", m_ui->actionShowTopView, DataView::Top);
    initSingleView(tr("Side View"), "leftView", m_ui->actionShowLeftView, DataView::Left);
    initSingleView(tr("Front View"), "frontView", m_ui->actionShowFrontView, DataView::Front);
}

void MainWindow::initSingleView(
        const QString &title,
        const QString &objectName,
        QAction *actionShow,
        DataView::Direction direction)
{
    DataView *dataView = new DataView;
    QDockWidget *dockWidget = new QDockWidget(title);
    dockWidget->setWidget(dataView);
    dockWidget->setObjectName(objectName);
    addDockWidget(Qt::BottomDockWidgetArea, dockWidget);

    dataView->setMainWindow(this);
    dataView->setDirection(direction);

    connect(actionShow, SIGNAL(toggled(bool)),
            dockWidget, SLOT(setVisible(bool)));
    connect(dockWidget, SIGNAL(visibilityChanged(bool)),
            actionShow, SLOT(setChecked(bool)));

    connect(this, SIGNAL(dataChanged()),
            dataView, SLOT(updateView()));
    connect(this, SIGNAL(rangeChanged(double, double)),
            dataView, SLOT(updateView()));
    connect(this, SIGNAL(rotationChanged(double)),
            dataView, SLOT(updateView()));
}

void MainWindow::closeEvent(
        QCloseEvent *event)
{
    // Save window state
    writeSettings();

    // Save plot state
    foreach (PlotValue *v, m_yValues)
    {
        v->writeSettings();
    }

    event->accept();
}

DataPoint MainWindow::interpolateDataT(
        double t)
{
    const int i1 = findIndexBelowT(t);
    const int i2 = findIndexAboveT(t);

    if (i1 < 0)
    {
        return m_data.first();
    }
    else if (i2 >= m_data.size())
    {
        return m_data.last();
    }
    else
    {
        const DataPoint &dp1 = m_data[i1];
        const DataPoint &dp2 = m_data[i2];
        return interpolate(dp1, dp2, (t - dp1.t) / (dp2.t - dp1.t));
    }
}

int MainWindow::findIndexBelowT(
        double t)
{
    for (int i = 0; i < m_data.size(); ++i)
    {
        DataPoint &dp = m_data[i];
        if (dp.t > t) return i - 1;
    }

    return m_data.size() - 1;
}

int MainWindow::findIndexAboveT(
        double t)
{
    for (int i = m_data.size() - 1; i >= 0; --i)
    {
        DataPoint &dp = m_data[i];
        if (dp.t <= t) return i + 1;
    }

    return 0;
}

void MainWindow::on_actionImport_triggered()
{
    const double pi = 3.14159265359;

    // Initialize settings object
    QSettings settings("FlySight", "Viewer");

    // Get last file read
    QString rootFolder = settings.value("folder").toString();

    QString fileName = QFileDialog::getOpenFileName(this, tr("Import"), rootFolder, tr("CSV Files (*.csv)"));

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        // TODO: Error message
        return;
    }

    // Remember last file read
    settings.setValue("folder", QFileInfo(fileName).absoluteFilePath());

    QTextStream in(&file);

    // skip first 2 rows
    if (!in.atEnd()) in.readLine();
    if (!in.atEnd()) in.readLine();

    m_data.clear();

    while (!in.atEnd())
    {
        QString line = in.readLine();
        QStringList cols = line.split(",");

        DataPoint pt;

        pt.dateTime = QDateTime::fromString(cols[0], Qt::ISODate);

        pt.lat   = cols[1].toDouble();
        pt.lon   = cols[2].toDouble();
        pt.hMSL  = cols[3].toDouble();

        pt.velN  = cols[4].toDouble();
        pt.velE  = cols[5].toDouble();
        pt.velD  = cols[6].toDouble();

        pt.hAcc  = cols[7].toDouble();
        pt.vAcc  = cols[8].toDouble();
        pt.sAcc  = cols[9].toDouble();

        pt.numSV = cols[11].toDouble();

        //
        // TODO: Handle newer CSV version
        //

        m_data.append(pt);
    }

    double dist2D = 0, dist3D = 0;

    QVector< double > dt;

    for (int i = 0; i < m_data.size(); ++i)
    {
        const DataPoint &dp0 = m_data[m_data.size() - 1];
        DataPoint &dp = m_data[i];

        double distance = getDistance(dp0, dp);
        double bearing = getBearing(dp0, dp);

        dp.x = distance * sin(bearing);
        dp.y = distance * cos(bearing);
        dp.z = dp.alt = dp.hMSL - dp0.hMSL;

        if (i > 0)
        {
            const DataPoint &dpPrev = m_data[i - 1];

            double dh = getDistance(dpPrev, dp);
            double dz = dp.hMSL - dpPrev.hMSL;

            dist2D += dh;
            dist3D += sqrt(dh * dh + dz * dz);

            dt.append(dp.t - dpPrev.t);
        }

        dp.dist2D = dist2D;
        dp.dist3D = dist3D;

        qint64 start = dp0.dateTime.toMSecsSinceEpoch();
        qint64 end = dp.dateTime.toMSecsSinceEpoch();

        dp.t = (double) (end - start) / 1000;
    }

    for (int i = 0; i < m_data.size(); ++i)
    {
        DataPoint &dp = m_data[i];
        dp.curv = getSlope(i, DiveAngle);
    }

    if (dt.size() > 0)
    {
        qSort(dt.begin(), dt.end());
        m_timeStep = dt.at(dt.size() / 2);
    }
    else
    {
        m_timeStep = 1.0;
    }

    initPlotData();
}

double MainWindow::getSlope(
        const int center,
        YAxisType yAxis) const
{
    int iMin = qMax (0, center - 2);
    int iMax = qMin (m_data.size () - 1, center + 2);

    double sumx = 0, sumy = 0, sumxx = 0, sumxy = 0;

    for (int i = iMin; i <= iMax; ++i)
    {
        const DataPoint &dp = m_data[i];
        double yValue = m_yValues[yAxis]->value(dp, m_units);

        sumx += dp.t;
        sumy += yValue;
        sumxx += dp.t * dp.t;
        sumxy += dp.t * yValue;
    }

    int n = iMax - iMin + 1;
    return (sumxy - sumx * sumy / n) / (sumxx - sumx * sumx / n);
}

double MainWindow::getDistance(
        const DataPoint &dp1,
        const DataPoint &dp2) const
{
    const double R = 6371009;
    const double pi = 3.14159265359;

    double lat1 = dp1.lat / 180 * pi;
    double lon1 = dp1.lon / 180 * pi;

    double lat2 = dp2.lat / 180 * pi;
    double lon2 = dp2.lon / 180 * pi;

    double dLat = lat2 - lat1;
    double dLon = lon2 - lon1;

    double a = sin(dLat / 2) * sin(dLat / 2) +
            sin(dLon / 2) * sin(dLon / 2) * cos(lat1) * cos(lat2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return R * c;
}

double MainWindow::getBearing(
        const DataPoint &dp1,
        const DataPoint &dp2) const
{
    const double pi = 3.14159265359;

    double lat1 = dp1.lat / 180 * pi;
    double lon1 = dp1.lon / 180 * pi;

    double lat2 = dp2.lat / 180 * pi;
    double lon2 = dp2.lon / 180 * pi;

    double dLon = lon2 - lon1;

    double y = sin(dLon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) -
            sin(lat1) * cos(lat2) * cos(dLon);

    return atan2(y, x);
}

void MainWindow::setMark(
        double start,
        double end)
{
    if (m_data.isEmpty()) return;

    mMarkStart = start;
    mMarkEnd = end;
    mMarkActive = true;

    emit dataChanged();
}

void MainWindow::setMark(
        double mark)
{
    if (m_data.isEmpty()) return;

    mMarkStart = mMarkEnd = mark;
    mMarkActive = true;

    emit dataChanged();
}

void MainWindow::clearMark()
{
    mMarkActive = false;

    emit dataChanged();
}

void MainWindow::initPlotData()
{
    double lower, upper;

    for (int i = 0; i < m_data.size(); ++i)
    {
        DataPoint &dp = m_data[i];

        if (i == 0)
        {
            lower = upper = dp.t;
        }
        else
        {
            if (dp.t < lower) lower = dp.t;
            if (dp.t > upper) upper = dp.t;
        }
    }

    setRange(lower, upper);

    emit dataChanged();
}

void MainWindow::on_actionElevation_triggered()
{
    m_yValues[Elevation]->setVisible(
                !m_yValues[Elevation]->visible());
    emit plotChanged();
}

void MainWindow::on_actionVerticalSpeed_triggered()
{
    m_yValues[VerticalSpeed]->setVisible(
                !m_yValues[VerticalSpeed]->visible());
    emit plotChanged();
}

void MainWindow::on_actionHorizontalSpeed_triggered()
{
    m_yValues[HorizontalSpeed]->setVisible(
                !m_yValues[HorizontalSpeed]->visible());
    emit plotChanged();
}

void MainWindow::on_actionTime_triggered()
{
    updateBottom(Time);
}

void MainWindow::on_actionDistance2D_triggered()
{
    updateBottom(Distance2D);
}

void MainWindow::on_actionDistance3D_triggered()
{
    updateBottom(Distance3D);
}

void MainWindow::updateBottom(
        XAxisType xAxis)
{
    double lower = mRangeLower;
    double upper = mRangeUpper;

    m_xAxis = xAxis;
    initPlotData();

    setRange(lower, upper);

    updateBottomActions();
}

void MainWindow::updateBottomActions()
{
    m_ui->actionTime->setChecked(m_xAxis == Time);
    m_ui->actionDistance2D->setChecked(m_xAxis == Distance2D);
    m_ui->actionDistance3D->setChecked(m_xAxis == Distance3D);
}

void MainWindow::updateLeftActions()
{
    m_ui->actionElevation->setChecked(m_yValues[Elevation]->visible());
    m_ui->actionVerticalSpeed->setChecked(m_yValues[VerticalSpeed]->visible());
    m_ui->actionHorizontalSpeed->setChecked(m_yValues[HorizontalSpeed]->visible());
    m_ui->actionTotalSpeed->setChecked(m_yValues[TotalSpeed]->visible());
    m_ui->actionDiveAngle->setChecked(m_yValues[DiveAngle]->visible());
    m_ui->actionCurvature->setChecked(m_yValues[Curvature]->visible());
    m_ui->actionGlideRatio->setChecked(m_yValues[GlideRatio]->visible());
    m_ui->actionHorizontalAccuracy->setChecked(m_yValues[HorizontalAccuracy]->visible());
    m_ui->actionVerticalAccuracy->setChecked(m_yValues[VerticalAccuracy]->visible());
    m_ui->actionSpeedAccuracy->setChecked(m_yValues[SpeedAccuracy]->visible());
    m_ui->actionNumberOfSatellites->setChecked(m_yValues[NumberOfSatellites]->visible());
}

void MainWindow::on_actionTotalSpeed_triggered()
{
    m_yValues[TotalSpeed]->setVisible(
                !m_yValues[TotalSpeed]->visible());
    emit plotChanged();
}

void MainWindow::on_actionDiveAngle_triggered()
{
    m_yValues[DiveAngle]->setVisible(
                !m_yValues[DiveAngle]->visible());
    emit plotChanged();
}

void MainWindow::on_actionCurvature_triggered()
{
    m_yValues[Curvature]->setVisible(
                !m_yValues[Curvature]->visible());
    emit plotChanged();
}

void MainWindow::on_actionGlideRatio_triggered()
{
    m_yValues[GlideRatio]->setVisible(
                !m_yValues[GlideRatio]->visible());
    emit plotChanged();
}

void MainWindow::on_actionHorizontalAccuracy_triggered()
{
    m_yValues[HorizontalAccuracy]->setVisible(
                !m_yValues[HorizontalAccuracy]->visible());
    emit plotChanged();
}

void MainWindow::on_actionVerticalAccuracy_triggered()
{
    m_yValues[VerticalAccuracy]->setVisible(
                !m_yValues[VerticalAccuracy]->visible());
    emit plotChanged();
}

void MainWindow::on_actionSpeedAccuracy_triggered()
{
    m_yValues[SpeedAccuracy]->setVisible(
                !m_yValues[SpeedAccuracy]->visible());
    emit plotChanged();
}

void MainWindow::on_actionNumberOfSatellites_triggered()
{
    m_yValues[NumberOfSatellites]->setVisible(
                !m_yValues[NumberOfSatellites]->visible());
    emit plotChanged();
}

void MainWindow::on_actionPan_triggered()
{
    setTool(Pan);
}

void MainWindow::on_actionZoom_triggered()
{
    setTool(Zoom);
}

void MainWindow::on_actionMeasure_triggered()
{
    setTool(Measure);
}

void MainWindow::on_actionZero_triggered()
{
    setTool(Zero);
}

void MainWindow::on_actionGround_triggered()
{
    setTool(Ground);
}

void MainWindow::on_actionImportGates_triggered()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this, tr("Import"), "", tr("CSV Files (*.csv)"));

    for (int i = 0; i < fileNames.size(); ++i)
    {
        QFile file(fileNames[i]);
        if (!file.open(QIODevice::ReadOnly))
        {
            // TODO: Error message
            continue;
        }

        QTextStream in(&file);

        // skip first 2 rows
        if (!in.atEnd()) in.readLine();
        if (!in.atEnd()) in.readLine();

        QVector< double > lat, lon, hMSL;

        while (!in.atEnd())
        {
            QString line = in.readLine();
            QStringList cols = line.split(",");

            lat.append(cols[1].toDouble());
            lon.append(cols[2].toDouble());
            hMSL.append(cols[3].toDouble());
        }

        if (lat.size() > 0)
        {
            qSort(lat.begin(), lat.end());
            qSort(lon.begin(), lon.end());
            qSort(hMSL.begin(), hMSL.end());

            int mid = lat.size() / 2;

            DataPoint dp ;

            dp.lat  = lat[mid];
            dp.lon  = lon[mid];
            dp.hMSL = hMSL[mid];

            m_waypoints.append(dp);
        }
    }

    emit dataChanged();
}

void MainWindow::on_actionPreferences_triggered()
{
    ConfigDialog dlg;

    dlg.setUnits(m_units);

    dlg.exec();

    if (m_units != dlg.units())
    {
        m_units = dlg.units();
        initPlotData();
    }
}

void MainWindow::setRange(
        double lower,
        double upper)
{
    mRangeLower = qMin(lower, upper);
    mRangeUpper = qMax(lower, upper);
    emit rangeChanged(lower, upper);
}

void MainWindow::setRotation(
        double rotation)
{
    m_viewDataRotation = rotation;
    emit rotationChanged(m_viewDataRotation);
}

void MainWindow::setZero(
        double t)
{
    if (m_data.isEmpty()) return;

    DataPoint dp0 = interpolateDataT(t);

    for (int i = 0; i < m_data.size(); ++i)
    {
        DataPoint &dp = m_data[i];

        dp.t -= dp0.t;
        dp.x -= dp0.x;
        dp.y -= dp0.y;
        dp.z -= dp0.z;

        dp.dist2D -= dp0.dist2D;
        dp.dist3D -= dp0.dist3D;
    }

    double lower = mRangeLower;
    double upper = mRangeUpper;

    initPlotData();

    setRange(lower - dp0.t, upper - dp0.t);
    setTool(mPrevTool);
}

void MainWindow::setGround(
        double t)
{
    if (m_data.isEmpty()) return;

    DataPoint dp0 = interpolateDataT(t);

    for (int i = 0; i < m_data.size(); ++i)
    {
        DataPoint &dp = m_data[i];
        dp.alt -= dp0.alt;
    }

    double lower = mRangeLower;
    double upper = mRangeUpper;

    initPlotData();

    setRange(lower, upper);
    setTool(mPrevTool);
}

void MainWindow::setTool(
        Tool tool)
{
    if (tool != Ground && tool != Zero)
    {
        mPrevTool = tool;
    }
    mTool = tool;

    m_ui->actionPan->setChecked(tool == Pan);
    m_ui->actionZoom->setChecked(tool == Zoom);
    m_ui->actionMeasure->setChecked(tool == Measure);
    m_ui->actionZero->setChecked(tool == Zero);
    m_ui->actionGround->setChecked(tool == Ground);
}
