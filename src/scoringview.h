#ifndef SCORINGVIEW_H
#define SCORINGVIEW_H

#include <QWidget>

namespace Ui {
    class ScoringView;
}

class MainWindow;
class PPCForm;

class ScoringView : public QWidget
{
    Q_OBJECT

public:
    explicit ScoringView(QWidget *parent = 0);
    ~ScoringView();

    void setMainWindow(MainWindow *mainWindow);

private:
    Ui::ScoringView *ui;

    PPCForm         *mPPCForm;

public slots:
    void updateView();

private slots:
    void changePage(int page);
};

#endif // SCORINGVIEW_H
