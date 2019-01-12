#ifndef BATLABWIDGET_H
#define BATLABWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include "batlabcore/batlablib.h"
#include "cellteststatuswidget.h"

class BatlabWidget : public QFrame
{
    Q_OBJECT
public:

    explicit BatlabWidget(batlabStatusInfo info, int latestFirmwareVersion, QFrame *parent = nullptr);

signals:

public slots:
    void updateFirmware();

private:

    QHBoxLayout *batlabLayout;
    QGridLayout *batlabInfoLayout;

    QLabel *serialNumberLabel;
    QLabel *serialNumberValueLabel;

    QLabel *portNameLabel;
    QLabel *portNameValueLabel;

    QLabel *externalPowerConnectedLabel;
    QLabel *externalPowerConnectedValueLabel;

    QLabel *firmwareVersionLabel;
    QLabel *firmwareVersionValueLabel;

    QPushButton *firmwareVersionUpdateButton;

    QGridLayout *batlabCellInfoLayout;

    CellTestStatusWidget* CellTestStatusWidgetList[4];

};

#endif // BATLABWIDGET_H
