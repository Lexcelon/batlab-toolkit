#ifndef NEWCELLPLAYLISTWIZARD_H
#define NEWCELLPLAYLISTWIZARD_H

#include <QWizard>
#include <QtWidgets>

#include "batlabcore/batlablib.h"
#include "batlabcore/cellplaylist.h"
#include "batlabcore/version.h"

class NewCellPlaylistWizard : public QWizard {
  Q_OBJECT

public:
  NewCellPlaylistWizard(QWidget *parent = 0);

  void accept() override;
  void savePlaylist();
  void skipToNextPage();

signals:
  void finished(CellPlaylist);

private:
  bool skipped;
  CellPlaylist m_playlist;
};

class IntroPage : public QWizardPage {
  Q_OBJECT

public:
  IntroPage(QWidget *parent = 0);

private:
  QLabel *label;
};

class BasicSetupPage : public QWizardPage {
  Q_OBJECT

public:
  BasicSetupPage(QWidget *parent = 0);
  void updateExampleCellName();

private:
  QGroupBox *selectChemistryBox;
  QRadioButton *lipoRadioButton;
  QRadioButton *ironPhosphateRadioButton;
  QRadioButton *otherRadioButton;
  QLabel *sameTypeLabel;
  QLabel *numCellsLabel;
  QSpinBox *numCellsSpinBox;
  QLabel *cellDesignatorLabel;
  QLineEdit *cellDesignatorLineEdit;
  QLabel *startingCellNumberLabel;
  QSpinBox *startingCellNumberSpinBox;
  QLabel *exampleCellNameLabel;
  QLabel *exampleCellName;
  QLabel *cellPlaylistNameLabel;
  QLineEdit *cellPlaylistNameLineEdit;
};

class ConfigPlaylistPage : public QWizardPage {
  Q_OBJECT

public:
  ConfigPlaylistPage(QWidget *parent = 0);
  void initializePage();
  void enableOrDisableStorageDischargeField();
  void updateDynamicFieldBounds();

private:
  QLabel *numWarmupCyclesLabel;
  QSpinBox *numWarmupCyclesSpinBox;

  QLabel *numMeasurementCyclesLabel;
  QSpinBox *numMeasurementCyclesSpinBox;

  QCheckBox *storageDischargeCheckBox;

  QWidget *advancedConfigExtensionWidget;

  QLabel *restPeriodLabel;
  QDoubleSpinBox *restPeriodSpinBox;
  QLabel *restPeriodUnit;

  QLabel *highVoltageCutoffLabel;
  QDoubleSpinBox *highVoltageCutoffSpinBox;
  QLabel *highVoltageCutoffUnit;

  QLabel *lowVoltageCutoffLabel;
  QDoubleSpinBox *lowVoltageCutoffSpinBox;
  QLabel *lowVoltageCutoffUnit;

  QLabel *chargeTemperatureCutoffLabel;
  QDoubleSpinBox *chargeTemperatureCutoffSpinBox;
  QLabel *chargeTemperatureCutoffUnit;

  QLabel *dischargeTemperatureCutoffLabel;
  QDoubleSpinBox *dischargeTemperatureCutoffSpinBox;
  QLabel *dischargeTemperatureCutoffUnit;

  QLabel *chargeCurrentSafetyCutoffLabel;
  QDoubleSpinBox *chargeCurrentSafetyCutoffSpinBox;
  QLabel *chargeCurrentSafetyCutoffUnit;

  QLabel *dischargeCurrentSafetyCutoffLabel;
  QDoubleSpinBox *dischargeCurrentSafetyCutoffSpinBox;
  QLabel *dischargeCurrentSafetyCutoffUnit;

  QLabel *prechargeRateLabel;
  QDoubleSpinBox *prechargeRateSpinBox;
  QLabel *prechargeRateUnit;

  QLabel *chargeRateLabel;
  QDoubleSpinBox *chargeRateSpinBox;
  QLabel *chargeRateUnit;

  QLabel *dischargeRateLabel;
  QDoubleSpinBox *dischargeRateSpinBox;
  QLabel *dischargeRateUnit;

  QLabel *storageDischargeVoltageLabel;
  QDoubleSpinBox *storageDischargeVoltageSpinBox;
  QLabel *storageDischargeVoltageUnit;

  QLabel *acceptableCellImpedanceThresholdLabel;
  QDoubleSpinBox *acceptableCellImpedanceThresholdSpinBox;
  QLabel *acceptableCellImpedanceThresholdUnit;

  QWidget *basicConfigWidget;
  QPushButton *advancedConfigButton;
};

class PlaylistDirectoryPage : public QWizardPage {
  Q_OBJECT

public:
  PlaylistDirectoryPage(QWidget *parent = 0);

private:
  QLineEdit *playlistDirectoryLineEdit;
  QPushButton *playlistDirectoryBrowseButton;

  void browseForPlaylistDirectory();
  void initializePage();
  bool validatePage();
};

class SavePlaylistPage : public QWizardPage {
  Q_OBJECT

public:
  SavePlaylistPage(QWidget *parent = 0);
  QPushButton *skipButton;

private:
  QLineEdit *saveFilenameLineEdit;
  QPushButton *saveFilenameBrowseButton;
  void browseForSaveFilename();
  void initializePage();
  bool validatePage();
};

class FinishPlaylistPage : public QWizardPage {
  Q_OBJECT

public:
  FinishPlaylistPage(QWidget *parent = 0);

private:
};

#endif // NEWCELLPLAYLISTWIZARD_H
