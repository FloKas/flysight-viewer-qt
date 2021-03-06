#include "configdialog.h"
#include "ui_configdialog.h"

#include <QComboBox>
#include <QSettings>
#include <QwwColorComboBox>

#include "dataplot.h"
#include "mainwindow.h"

#define PLOT_COLUMN_COLOUR  0
#define PLOT_COLUMN_MIN     1
#define PLOT_COLUMN_MAX     2
#define PLOT_NUM_COLUMNS    3

ConfigDialog::ConfigDialog(MainWindow *mainWindow) :
    QDialog(mainWindow),
    ui(new Ui::ConfigDialog)
{
    ui->setupUi(this);

    // Restore window geometry
    QSettings settings("FlySight", "Viewer");
    settings.beginGroup("configDialog");
    restoreGeometry(settings.value("geometry").toByteArray());
    settings.endGroup();

    // Add page
    ui->contentsWidget->addItems(
                QStringList() << tr("General"));
    ui->contentsWidget->addItems(
                QStringList() << tr("Aerodynamics"));
    ui->contentsWidget->addItems(
                QStringList() << tr("Plots"));
    ui->contentsWidget->addItems(
                QStringList() << tr("Wind"));

    // Add units
    ui->unitsCombo->addItems(
                QStringList() << tr("Metric") << tr("Imperial"));

    // Update plot widget
    updatePlots();

    // Update plots when units are changed
    connect(ui->unitsCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(updatePlots()));

    // Connect contents panel to stacked widget
    connect(ui->contentsWidget,
            SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            this, SLOT(changePage(QListWidgetItem*,QListWidgetItem*)));

    // Go to first page
    ui->contentsWidget->setCurrentRow(0);
}

ConfigDialog::~ConfigDialog()
{
    // Save window geometry
    QSettings settings("FlySight", "Viewer");
    settings.beginGroup("configDialog");
    settings.setValue("geometry", saveGeometry());
    settings.endGroup();

    delete ui;
}

void ConfigDialog::changePage(
        QListWidgetItem *current,
        QListWidgetItem *previous)
{
    if (!current) current = previous;
    ui->pagesWidget->setCurrentIndex(
                ui->contentsWidget->row(current));
}

void ConfigDialog::updatePlots()
{
    MainWindow *mainWindow = (MainWindow *) parent();

    // Color list
    QStringList colorNames = QColor::colorNames();

    // Set up plots widget
    ui->plotTable->setColumnCount(PLOT_NUM_COLUMNS);
    ui->plotTable->setRowCount(DataPlot::yaLast);

    ui->plotTable->setHorizontalHeaderLabels(
                QStringList() << tr("Colour") << tr("Minimum") << tr("Maximum"));

    QStringList verticalHeaderLabels;

    for (int i = 0; i < DataPlot::yaLast; ++i)
    {
        PlotValue *yValue = mainWindow->plotArea()->yValue(i);

        verticalHeaderLabels << yValue->title(units());

        if (!ui->plotTable->cellWidget(i, PLOT_COLUMN_COLOUR))
        {
            QwwColorComboBox *combo = new QwwColorComboBox();
            foreach (const QString &colorName, colorNames)
            {
                const QColor &color(colorName);
                combo->addColor(color, colorName);
                if (color == yValue->color())
                {
                    combo->setCurrentText(colorName);
                }
            }
            ui->plotTable->setCellWidget(i, PLOT_COLUMN_COLOUR, combo);
        }

        // Conversion factor from internal units to display units
        const double factor = yValue->factor(units());

        QTableWidgetItem *item;
        if (!(item = ui->plotTable->item(i, PLOT_COLUMN_MIN)))
        {
            item = new QTableWidgetItem;
            ui->plotTable->setItem(i, PLOT_COLUMN_MIN, item);
        }
        item->setText(QString::number(yValue->minimum() * factor));
        item->setCheckState(yValue->useMinimum() ? Qt::Checked : Qt::Unchecked);

        if (!(item = ui->plotTable->item(i, PLOT_COLUMN_MAX)))
        {
            item = new QTableWidgetItem;
            ui->plotTable->setItem(i, PLOT_COLUMN_MAX, item);
        }
        item->setText(QString::number(yValue->maximum() * factor));
        item->setCheckState(yValue->useMaximum() ? Qt::Checked : Qt::Unchecked);
    }

    ui->plotTable->setVerticalHeaderLabels(verticalHeaderLabels);
}

void ConfigDialog::setUnits(
        PlotValue::Units units)
{
    ui->unitsCombo->setCurrentIndex(units);
}

PlotValue::Units ConfigDialog::units() const
{
    return (PlotValue::Units) ui->unitsCombo->currentIndex();
}

void ConfigDialog::setGroundReference(
        MainWindow::GroundReference ref)
{
    ui->autoReferenceButton->setChecked(ref == MainWindow::Automatic);
    ui->fixedReferenceButton->setChecked(ref == MainWindow::Fixed);
}

double ConfigDialog::fixedReference() const
{
    return ui->fixedReferenceEdit->text().toDouble();
}

void ConfigDialog::setFixedReference(
        double elevation)
{
    ui->fixedReferenceEdit->setText(QString("%1").arg(elevation));
}

MainWindow::GroundReference ConfigDialog::groundReference() const
{
    if (ui->autoReferenceButton->isChecked()) return MainWindow::Automatic;
    else                                      return MainWindow::Fixed;
}

void ConfigDialog::setMass(
        double mass)
{
    ui->massEdit->setText(QString("%1").arg(mass));
}

double ConfigDialog::mass() const
{
    return ui->massEdit->text().toDouble();
}

void ConfigDialog::setPlanformArea(
        double area)
{
    ui->areaEdit->setText(QString("%1").arg(area));
}

double ConfigDialog::planformArea() const
{
    return ui->areaEdit->text().toDouble();
}

void ConfigDialog::setMinDrag(
        double minDrag)
{
    ui->minDragEdit->setText(QString("%1").arg(minDrag));
}

double ConfigDialog::minDrag() const
{
    return ui->minDragEdit->text().toDouble();
}

void ConfigDialog::setMinLift(
        double minLift)
{
    ui->minLiftEdit->setText(QString("%1").arg(minLift));
}

double ConfigDialog::minLift() const
{
    return ui->minLiftEdit->text().toDouble();
}

void ConfigDialog::setMaxLift(
        double maxLift)
{
    ui->maxLiftEdit->setText(QString("%1").arg(maxLift));
}

double ConfigDialog::maxLift() const
{
    return ui->maxLiftEdit->text().toDouble();
}

void ConfigDialog::setMaxLD(
        double maxLD)
{
    ui->maxLDEdit->setText(QString("%1").arg(maxLD));
}

double ConfigDialog::maxLD() const
{
    return ui->maxLDEdit->text().toDouble();
}

void ConfigDialog::setSimulationTime(
        int simulationTime)
{
    ui->simTimeSpinBox->setValue(simulationTime);
}

int ConfigDialog::simulationTime() const
{
    return ui->simTimeSpinBox->value();
}

QColor ConfigDialog::plotColor(
        int i) const
{
    QComboBox *combo = (QComboBox *) ui->plotTable->cellWidget(i, PLOT_COLUMN_COLOUR);
    return QColor(combo->currentText());
}

double ConfigDialog::plotMinimum(
        int i) const
{
    return ui->plotTable->item(i, PLOT_COLUMN_MIN)->text().toDouble();
}

double ConfigDialog::plotMaximum(
        int i) const
{
    return ui->plotTable->item(i, PLOT_COLUMN_MAX)->text().toDouble();
}

bool ConfigDialog::plotUseMinimum(
        int i) const
{
    return ui->plotTable->item(i, PLOT_COLUMN_MIN)->checkState() == Qt::Checked;
}

bool ConfigDialog::plotUseMaximum(
        int i) const
{
    return ui->plotTable->item(i, PLOT_COLUMN_MAX)->checkState() == Qt::Checked;
}

void ConfigDialog::setLineThickness(
        double width)
{
    ui->lineThicknessEdit->setText(QString::number(width));
}

double ConfigDialog::lineThickness() const
{
    return ui->lineThicknessEdit->text().toDouble();
}

void ConfigDialog::setWindSpeed(
        double speed)
{
    ui->windSpeedEdit->setText(QString::number(speed));
}

double ConfigDialog::windSpeed() const
{
    return ui->windSpeedEdit->text().toDouble();
}

void ConfigDialog::setWindUnits(
        QString units)
{
    ui->windUnitsLabel->setText(units);
}

void ConfigDialog::setWindDirection(
        double dir)
{
    ui->windDirectionEdit->setText(QString::number(dir));
}

double ConfigDialog::windDirection() const
{
    return ui->windDirectionEdit->text().toDouble();
}
