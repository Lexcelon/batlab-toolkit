#include "playlistsettingswidget.h"

PlaylistSettingsWidget::PlaylistSettingsWidget(QWidget *parent)
    : QWidget(parent) {
  newCellPlaylistButton = new QPushButton(tr("New"));
  connect(newCellPlaylistButton, &QPushButton::clicked, this,
          &PlaylistSettingsWidget::newPlaylist);
  openCellPlaylistButton = new QPushButton(tr("Open"));
  connect(openCellPlaylistButton, &QPushButton::clicked, this,
          &PlaylistSettingsWidget::openPlaylist);

  cellPlaylistNameLabel = new QLabel(tr("Playlist name:"));
  cellPlaylistNameLineEdit = new QLineEdit;
  // Only valid characters
  QRegExp cellPlaylistNameRx("^[ \\w\\-\\.]+$");
  QValidator *cellPlaylistNameValidator =
      new QRegExpValidator(cellPlaylistNameRx);
  cellPlaylistNameLineEdit->setValidator(cellPlaylistNameValidator);
  connect(cellPlaylistNameLineEdit, &QLineEdit::textChanged, this,
          &PlaylistSettingsWidget::updatePlaylist);

  selectChemistryBox = new QGroupBox(tr("Cell chemistry type"));
  lipoRadioButton = new QRadioButton(
      tr("3.7V Lithium-Ion (LiPo, LIP ,Li-Poly, LCO, LMO, NMC, NCA)"));
  ironPhosphateRadioButton =
      new QRadioButton(tr("3.2V Lithium Iron Phosphate (LiFePO4, LFP)"));
  otherRadioButton = new QRadioButton(tr("Other"));

  lipoRadioButton->setChecked(true);

  QVBoxLayout *groupBoxLayout = new QVBoxLayout;
  groupBoxLayout->addWidget(lipoRadioButton);
  groupBoxLayout->addWidget(ironPhosphateRadioButton);
  groupBoxLayout->addWidget(otherRadioButton);
  selectChemistryBox->setLayout(groupBoxLayout);
  connect(lipoRadioButton, &QRadioButton::toggled, this,
          &PlaylistSettingsWidget::updateBoundsBasedOnChemistryType);
  connect(ironPhosphateRadioButton, &QRadioButton::toggled, this,
          &PlaylistSettingsWidget::updateBoundsBasedOnChemistryType);
  connect(otherRadioButton, &QRadioButton::toggled, this,
          &PlaylistSettingsWidget::updateBoundsBasedOnChemistryType);
  connect(lipoRadioButton, &QRadioButton::toggled, this,
          &PlaylistSettingsWidget::updatePlaylist);
  connect(ironPhosphateRadioButton, &QRadioButton::toggled, this,
          &PlaylistSettingsWidget::updatePlaylist);
  connect(otherRadioButton, &QRadioButton::toggled, this,
          &PlaylistSettingsWidget::updatePlaylist);

  sameTypeLabel = new QLabel(
      tr("Please note that all cells in a playlist must be of the same type."));
  sameTypeLabel->setWordWrap(true);

  cellNamesListLabel = new QLabel(tr("Cell names:"));
  cellNamesListWidget = new QListWidget(this);
  connect(cellNamesListWidget, &QListWidget::itemChanged, this,
          &PlaylistSettingsWidget::updatePlaylist);

  numWarmupCyclesLabel = new QLabel(tr("Number of warmup cycles:"));
  numWarmupCyclesSpinBox = new QSpinBox;
  numWarmupCyclesSpinBox->setMinimum(NUM_WARMUP_CYCLES_MIN);
  numWarmupCyclesSpinBox->setMaximum(NUM_WARMUP_CYCLES_MAX);
  numWarmupCyclesSpinBox->setValue(NUM_WARMUP_CYCLES_DEFAULT);
  connect(numWarmupCyclesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &PlaylistSettingsWidget::updatePlaylist);

  numMeasurementCyclesLabel = new QLabel(tr("Number of measurement cycles:"));
  numMeasurementCyclesSpinBox = new QSpinBox;
  numMeasurementCyclesSpinBox->setMinimum(NUM_MEASUREMENT_CYCLES_MIN);
  numMeasurementCyclesSpinBox->setMaximum(NUM_MEASUREMENT_CYCLES_MAX);
  numMeasurementCyclesSpinBox->setValue(NUM_MEASUREMENT_CYCLES_DEFAULT);
  connect(numMeasurementCyclesSpinBox,
          QOverload<int>::of(&QSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  storageDischargeCheckBox =
      new QCheckBox(tr("Discharge to storage voltage after testing"));
  storageDischargeCheckBox->setChecked(STORAGE_DISCHARGE_DEFAULT);
  connect(storageDischargeCheckBox, &QCheckBox::toggled, this,
          &PlaylistSettingsWidget::enableOrDisableStorageDischargeField);
  connect(storageDischargeCheckBox, &QCheckBox::toggled, this,
          &PlaylistSettingsWidget::updatePlaylist);

  restPeriodLabel = new QLabel(tr("Rest period:"));
  restPeriodSpinBox = new QDoubleSpinBox;
  restPeriodUnit = new QLabel(tr("sec"));
  restPeriodSpinBox->setMinimum(REST_PERIOD_MIN);
  restPeriodSpinBox->setMaximum(REST_PERIOD_MAX);
  restPeriodSpinBox->setValue(REST_PERIOD_DEFAULT);
  connect(restPeriodSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  highVoltageCutoffLabel = new QLabel(tr("High voltage cutoff:"));
  highVoltageCutoffSpinBox = new QDoubleSpinBox;
  highVoltageCutoffUnit = new QLabel(tr("V"));
  highVoltageCutoffSpinBox->setSingleStep(0.1);
  highVoltageCutoffSpinBox->setMinimum(HIGH_VOLTAGE_CUTOFF_MIN);
  highVoltageCutoffSpinBox->setMaximum(HIGH_VOLTAGE_CUTOFF_MAX);
  highVoltageCutoffSpinBox->setValue(HIGH_VOLTAGE_CUTOFF_DEFAULT);
  connect(highVoltageCutoffSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updateDynamicFieldBounds);
  connect(highVoltageCutoffSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  lowVoltageCutoffLabel = new QLabel(tr("Low voltage cutoff:"));
  lowVoltageCutoffSpinBox = new QDoubleSpinBox;
  lowVoltageCutoffUnit = new QLabel(tr("V"));
  lowVoltageCutoffSpinBox->setSingleStep(0.1);
  lowVoltageCutoffSpinBox->setMinimum(LOW_VOLTAGE_CUTOFF_MIN);
  lowVoltageCutoffSpinBox->setMaximum(LOW_VOLTAGE_CUTOFF_MAX);
  lowVoltageCutoffSpinBox->setValue(LOW_VOLTAGE_CUTOFF_DEFAULT);
  connect(lowVoltageCutoffSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  chargeTemperatureCutoffLabel = new QLabel(tr("Charge temperature cutoff:"));
  chargeTemperatureCutoffSpinBox = new QDoubleSpinBox;
  chargeTemperatureCutoffUnit = new QLabel(tr("°C"));
  chargeTemperatureCutoffSpinBox->setMinimum(CHARGE_TEMP_CUTOFF_MIN);
  chargeTemperatureCutoffSpinBox->setMaximum(CHARGE_TEMP_CUTOFF_MAX);
  chargeTemperatureCutoffSpinBox->setValue(CHARGE_TEMP_CUTOFF_DEFAULT);
  connect(chargeTemperatureCutoffSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  dischargeTemperatureCutoffLabel =
      new QLabel(tr("Discharge temperature cutoff:"));
  dischargeTemperatureCutoffSpinBox = new QDoubleSpinBox;
  dischargeTemperatureCutoffUnit = new QLabel(tr("°C"));
  dischargeTemperatureCutoffSpinBox->setMinimum(DISCHARGE_TEMP_CUTOFF_MIN);
  dischargeTemperatureCutoffSpinBox->setMaximum(DISCHARGE_TEMP_CUTOFF_MAX);
  dischargeTemperatureCutoffSpinBox->setValue(DISCHARGE_TEMP_CUTOFF_DEFAULT);
  connect(dischargeTemperatureCutoffSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  chargeCurrentSafetyCutoffLabel =
      new QLabel(tr("Charge current safety cutoff:"));
  chargeCurrentSafetyCutoffSpinBox = new QDoubleSpinBox;
  chargeCurrentSafetyCutoffUnit = new QLabel(tr("A"));
  chargeCurrentSafetyCutoffSpinBox->setSingleStep(0.1);
  chargeCurrentSafetyCutoffSpinBox->setDecimals(3);
  chargeCurrentSafetyCutoffSpinBox->setMinimum(
      CHARGE_CURRENT_SAFETY_CUTOFF_MIN);
  chargeCurrentSafetyCutoffSpinBox->setMaximum(
      CHARGE_CURRENT_SAFETY_CUTOFF_MAX);
  chargeCurrentSafetyCutoffSpinBox->setValue(
      CHARGE_CURRENT_SAFETY_CUTOFF_DEFAULT);
  connect(chargeCurrentSafetyCutoffSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updateDynamicFieldBounds);
  connect(chargeCurrentSafetyCutoffSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  dischargeCurrentSafetyCutoffLabel =
      new QLabel(tr("Discharge current safety cutoff:"));
  dischargeCurrentSafetyCutoffSpinBox = new QDoubleSpinBox;
  dischargeCurrentSafetyCutoffUnit = new QLabel(tr("A"));
  dischargeCurrentSafetyCutoffSpinBox->setSingleStep(0.1);
  dischargeCurrentSafetyCutoffSpinBox->setDecimals(3);
  dischargeCurrentSafetyCutoffSpinBox->setMinimum(
      DISCHARGE_CURRENT_SAFETY_CUTOFF_MIN);
  dischargeCurrentSafetyCutoffSpinBox->setMaximum(
      DISCHARGE_CURRENT_SAFETY_CUTOFF_MAX);
  dischargeCurrentSafetyCutoffSpinBox->setValue(
      DISCHARGE_CURRENT_SAFETY_CUTOFF_DEFAULT);
  connect(dischargeCurrentSafetyCutoffSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updateDynamicFieldBounds);
  connect(dischargeCurrentSafetyCutoffSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  prechargeRateLabel = new QLabel(tr("Precharge rate:"));
  prechargeRateSpinBox = new QDoubleSpinBox;
  prechargeRateUnit = new QLabel(tr("A"));
  prechargeRateSpinBox->setSingleStep(0.1);
  prechargeRateSpinBox->setDecimals(3);
  prechargeRateSpinBox->setMinimum(PRECHARGE_RATE_MIN);
  prechargeRateSpinBox->setMaximum(
      CHARGE_CURRENT_SAFETY_CUTOFF_DEFAULT); // Max is updated dynamically to
                                             // not be greater than the charge
                                             // current safety cutoff
  prechargeRateSpinBox->setValue(PRECHARGE_RATE_DEFAULT);
  connect(prechargeRateSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  chargeRateLabel = new QLabel(tr("Charge rate:"));
  chargeRateSpinBox = new QDoubleSpinBox;
  chargeRateUnit = new QLabel(tr("A"));
  chargeRateSpinBox->setSingleStep(0.1);
  chargeRateSpinBox->setDecimals(3);
  chargeRateSpinBox->setMinimum(CHARGE_RATE_MIN);
  chargeRateSpinBox->setMaximum(
      CHARGE_CURRENT_SAFETY_CUTOFF_DEFAULT); // Max is updated dynamically to
                                             // not be greater than the charge
                                             // current safety cutoff
  chargeRateSpinBox->setValue(CHARGE_RATE_DEFAULT);
  connect(chargeRateSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  dischargeRateLabel = new QLabel(tr("Discharge rate:"));
  dischargeRateSpinBox = new QDoubleSpinBox;
  dischargeRateUnit = new QLabel(tr("A"));
  dischargeRateSpinBox->setSingleStep(0.1);
  dischargeRateSpinBox->setDecimals(3);
  dischargeRateSpinBox->setMinimum(DISCHARGE_RATE_MIN);
  dischargeRateSpinBox->setMaximum(
      DISCHARGE_CURRENT_SAFETY_CUTOFF_DEFAULT); // Max is updated dynamically to
                                                // not be greater than the
                                                // discharge current safety
                                                // cutoff
  dischargeRateSpinBox->setValue(DISCHARGE_RATE_DEFAULT);
  connect(dischargeRateSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  storageDischargeVoltageLabel = new QLabel(tr("Storage discharge voltage:"));
  storageDischargeVoltageSpinBox = new QDoubleSpinBox;
  storageDischargeVoltageUnit = new QLabel(tr("V"));
  storageDischargeVoltageSpinBox->setSingleStep(0.1);
  storageDischargeVoltageSpinBox->setRange(
      STORAGE_DISCHARGE_VOLTAGE_MIN,
      HIGH_VOLTAGE_CUTOFF_DEFAULT); // Max is updated dynamically to not be
                                    // greater than the high voltage cutoff
  storageDischargeVoltageSpinBox->setValue(STORAGE_DISCHARGE_VOLTAGE_DEFAULT);
  connect(storageDischargeVoltageSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  acceptableCellImpedanceThresholdLabel =
      new QLabel(tr("Acceptable cell impedance threshold:"));
  acceptableCellImpedanceThresholdSpinBox = new QDoubleSpinBox;
  acceptableCellImpedanceThresholdUnit = new QLabel(tr("Ω"));
  acceptableCellImpedanceThresholdSpinBox->setSingleStep(0.1);
  acceptableCellImpedanceThresholdSpinBox->setRange(
      ACCEPTABLE_IMPEDANCE_THRESHOLD_MIN, ACCEPTABLE_IMPEDANCE_THRESHOLD_MAX);
  acceptableCellImpedanceThresholdSpinBox->setValue(
      ACCEPTABLE_IMPEDANCE_THRESHOLD_DEFAULT);
  connect(acceptableCellImpedanceThresholdSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PlaylistSettingsWidget::updatePlaylist);

  enableConstantVoltageCheckBox = new QCheckBox(tr("Enable constant voltage"));
  enableConstantVoltageCheckBox->setChecked(ENABLE_CONSTANT_VOLTAGE_DEFAULT);
  connect(enableConstantVoltageCheckBox, &QCheckBox::toggled, this,
          &PlaylistSettingsWidget::updatePlaylist);

  enableTrickleCheckBox = new QCheckBox(tr("Enable trickle charge/discharge"));
  enableTrickleCheckBox->setChecked(ENABLE_TRICKLE_DEFAULT);
  connect(enableTrickleCheckBox, &QCheckBox::toggled, this,
          &PlaylistSettingsWidget::updatePlaylist);

  enablePulseCheckBox = new QCheckBox(tr("Enable pulse charge/discharge"));
  enablePulseCheckBox->setChecked(ENABLE_PULSE_DEFAULT);
  connect(enablePulseCheckBox, &QCheckBox::toggled, this,
          &PlaylistSettingsWidget::updatePlaylist);

  QVBoxLayout *manageLayout = new QVBoxLayout;
  manageLayout->addWidget(newCellPlaylistButton);
  manageLayout->addWidget(openCellPlaylistButton);
  manageLayout->addStretch();

  QGridLayout *setupLayout = new QGridLayout;
  setupLayout->addWidget(cellPlaylistNameLabel, 0, 0);
  setupLayout->addWidget(cellPlaylistNameLineEdit, 0, 1);
  setupLayout->addWidget(selectChemistryBox, 1, 0, 1, 2);
  setupLayout->addWidget(sameTypeLabel, 2, 0, 1, 2);
  setupLayout->setRowStretch(3, 1);
  setupLayout->addWidget(cellNamesListLabel, 4, 0);
  setupLayout->addWidget(cellNamesListWidget, 5, 0, 1, 2);
  setupLayout->setRowStretch(6, 8);

  QGridLayout *basicConfigLayout = new QGridLayout;
  basicConfigLayout->addWidget(numWarmupCyclesLabel, 0, 0);
  basicConfigLayout->addWidget(numWarmupCyclesSpinBox, 0, 1);
  basicConfigLayout->addWidget(numMeasurementCyclesLabel, 1, 0);
  basicConfigLayout->addWidget(numMeasurementCyclesSpinBox, 1, 1);
  basicConfigLayout->addWidget(storageDischargeCheckBox, 2, 0);

  QGridLayout *advancedExtensionLayout = new QGridLayout;
  advancedExtensionLayout->addWidget(restPeriodLabel, 0, 0);
  advancedExtensionLayout->addWidget(restPeriodSpinBox, 0, 1);
  advancedExtensionLayout->addWidget(restPeriodUnit, 0, 2);
  advancedExtensionLayout->addWidget(highVoltageCutoffLabel, 1, 0);
  advancedExtensionLayout->addWidget(highVoltageCutoffSpinBox, 1, 1);
  advancedExtensionLayout->addWidget(highVoltageCutoffUnit, 1, 2);
  advancedExtensionLayout->addWidget(lowVoltageCutoffLabel, 2, 0);
  advancedExtensionLayout->addWidget(lowVoltageCutoffSpinBox, 2, 1);
  advancedExtensionLayout->addWidget(lowVoltageCutoffUnit, 2, 2);
  advancedExtensionLayout->addWidget(chargeTemperatureCutoffLabel, 3, 0);
  advancedExtensionLayout->addWidget(chargeTemperatureCutoffSpinBox, 3, 1);
  advancedExtensionLayout->addWidget(chargeTemperatureCutoffUnit, 3, 2);
  advancedExtensionLayout->addWidget(dischargeTemperatureCutoffLabel, 4, 0);
  advancedExtensionLayout->addWidget(dischargeTemperatureCutoffSpinBox, 4, 1);
  advancedExtensionLayout->addWidget(dischargeTemperatureCutoffUnit, 4, 2);
  advancedExtensionLayout->addWidget(chargeCurrentSafetyCutoffLabel, 5, 0);
  advancedExtensionLayout->addWidget(chargeCurrentSafetyCutoffSpinBox, 5, 1);
  advancedExtensionLayout->addWidget(chargeCurrentSafetyCutoffUnit, 5, 2);
  advancedExtensionLayout->addWidget(dischargeCurrentSafetyCutoffLabel, 6, 0);
  advancedExtensionLayout->addWidget(dischargeCurrentSafetyCutoffSpinBox, 6, 1);
  advancedExtensionLayout->addWidget(dischargeCurrentSafetyCutoffUnit, 6, 2);
  advancedExtensionLayout->addWidget(prechargeRateLabel, 7, 0);
  advancedExtensionLayout->addWidget(prechargeRateSpinBox, 7, 1);
  advancedExtensionLayout->addWidget(prechargeRateUnit, 7, 2);
  advancedExtensionLayout->addWidget(chargeRateLabel, 8, 0);
  advancedExtensionLayout->addWidget(chargeRateSpinBox, 8, 1);
  advancedExtensionLayout->addWidget(chargeRateUnit, 8, 2);
  advancedExtensionLayout->addWidget(dischargeRateLabel, 9, 0);
  advancedExtensionLayout->addWidget(dischargeRateSpinBox, 9, 1);
  advancedExtensionLayout->addWidget(dischargeRateUnit, 9, 2);
  advancedExtensionLayout->addWidget(storageDischargeVoltageLabel, 10, 0);
  advancedExtensionLayout->addWidget(storageDischargeVoltageSpinBox, 10, 1);
  advancedExtensionLayout->addWidget(storageDischargeVoltageUnit, 10, 2);
  advancedExtensionLayout->addWidget(acceptableCellImpedanceThresholdLabel, 11,
                                     0);
  advancedExtensionLayout->addWidget(acceptableCellImpedanceThresholdSpinBox,
                                     11, 1);
  advancedExtensionLayout->addWidget(acceptableCellImpedanceThresholdUnit, 11,
                                     2);
  advancedExtensionLayout->addWidget(enableConstantVoltageCheckBox, 12, 0);
  advancedExtensionLayout->addWidget(enableTrickleCheckBox, 13, 0);
  advancedExtensionLayout->addWidget(enablePulseCheckBox, 14, 0);

  QVBoxLayout *configLayout = new QVBoxLayout;
  configLayout->addLayout(basicConfigLayout);
  configLayout->addLayout(advancedExtensionLayout);
  configLayout->addStretch(1);

  QHBoxLayout *layout = new QHBoxLayout;
  layout->addLayout(manageLayout);
  layout->addLayout(setupLayout);
  layout->addStretch(1);
  layout->addLayout(configLayout);
  layout->addStretch(4);
  setLayout(layout);
}

void PlaylistSettingsWidget::enableOrDisableStorageDischargeField() {
  if (storageDischargeCheckBox->isChecked()) {
    storageDischargeVoltageSpinBox->setEnabled(true);
  } else {
    storageDischargeVoltageSpinBox->setEnabled(false);
  }
}

void PlaylistSettingsWidget::updateDynamicFieldBounds() {
  prechargeRateSpinBox->setMaximum(chargeCurrentSafetyCutoffSpinBox->value());
  chargeRateSpinBox->setMaximum(chargeCurrentSafetyCutoffSpinBox->value());
  dischargeRateSpinBox->setMaximum(
      dischargeCurrentSafetyCutoffSpinBox->value());
  storageDischargeVoltageSpinBox->setMaximum(highVoltageCutoffSpinBox->value());
}

void PlaylistSettingsWidget::updateBoundsBasedOnChemistryType() {
  if (ironPhosphateRadioButton->isChecked()) {
    highVoltageCutoffSpinBox->setMaximum(
        IRON_PHOSPHATE_HIGH_VOLTAGE_CUTOFF_MAX);
    highVoltageCutoffSpinBox->setValue(
        IRON_PHOSPHATE_HIGH_VOLTAGE_CUTOFF_DEFAULT);
    lowVoltageCutoffSpinBox->setValue(
        IRON_PHOSPHATE_LOW_VOLTAGE_CUTOFF_DEFAULT);
    storageDischargeVoltageSpinBox->setValue(
        IRON_PHOSPHATE_STORAGE_DISCHARGE_VOLTAGE_DEFAULT);
  } else {
    highVoltageCutoffSpinBox->setMaximum(HIGH_VOLTAGE_CUTOFF_MAX);
    highVoltageCutoffSpinBox->setValue(HIGH_VOLTAGE_CUTOFF_DEFAULT);
    lowVoltageCutoffSpinBox->setValue(LOW_VOLTAGE_CUTOFF_DEFAULT);
    storageDischargeVoltageSpinBox->setValue(STORAGE_DISCHARGE_VOLTAGE_DEFAULT);
  }
}

void PlaylistSettingsWidget::updatePlaylist() {
  CellPlaylist playlist;
  playlist.setCellPlaylistName(cellPlaylistNameLineEdit->text());
  if (lipoRadioButton->isChecked()) {
    playlist.setCellChemistryType(LIPO_CHEMISTRY_FIELDSTR);
  } else if (ironPhosphateRadioButton->isChecked()) {
    playlist.setCellChemistryType(IRON_PHOSPHATE_CHEMISTRY_FIELDSTR);
  } else if (otherRadioButton->isChecked()) {
    playlist.setCellChemistryType(OTHER_CHEMISTRY_FIELDSTR);
  }
  QVector<QString> cellNames;
  for (int i = 0; i < cellNamesListWidget->count(); i++) {
    cellNames.append(cellNamesListWidget->item(i)->text());
  }
  playlist.setCellNames(cellNames);
  playlist.setNumWarmupCycles(numWarmupCyclesSpinBox->value());
  playlist.setNumMeasurementCycles(numMeasurementCyclesSpinBox->value());
  playlist.setStorageDischarge(storageDischargeCheckBox->isChecked());
  playlist.setRestPeriod(restPeriodSpinBox->value());
  playlist.setHighVoltageCutoff(highVoltageCutoffSpinBox->value());
  playlist.setLowVoltageCutoff(lowVoltageCutoffSpinBox->value());
  playlist.setChargeTempCutoff(chargeTemperatureCutoffSpinBox->value());
  playlist.setDischargeTempCutoff(dischargeTemperatureCutoffSpinBox->value());
  playlist.setChargeCurrentSafetyCutoff(
      chargeCurrentSafetyCutoffSpinBox->value());
  playlist.setDischargeCurrentSafetyCutoff(
      dischargeCurrentSafetyCutoffSpinBox->value());
  playlist.setPrechargeRate(prechargeRateSpinBox->value());
  playlist.setChargeRate(chargeRateSpinBox->value());
  playlist.setDischargeRate(dischargeRateSpinBox->value());
  playlist.setStorageDischargeVoltage(storageDischargeVoltageSpinBox->value());
  playlist.setAcceptableImpedanceThreshold(
      acceptableCellImpedanceThresholdSpinBox->value());

  playlist.setEnableConstantVoltage(enableConstantVoltageCheckBox->isChecked());
  playlist.setEnableTrickle(enableTrickleCheckBox->isChecked());
  playlist.setEnablePulse(enablePulseCheckBox->isChecked());

  emit playlistUpdated(playlist);
}

void PlaylistSettingsWidget::loadPlaylist(CellPlaylist playlist) {
  cellPlaylistNameLineEdit->setText(playlist.getCellPlaylistName());

  if (playlist.getCellChemistryType() == LIPO_CHEMISTRY_FIELDSTR) {
    lipoRadioButton->setChecked(true);
  } else if (playlist.getCellChemistryType() ==
             IRON_PHOSPHATE_CHEMISTRY_FIELDSTR) {
    ironPhosphateRadioButton->setChecked(true);
  } else {
    otherRadioButton->setChecked(true);
  }

  cellNamesListWidget->clear();
  for (auto cellName : playlist.getCellNames()) {
    cellNamesListWidget->addItem(cellName);
  }

  numWarmupCyclesSpinBox->setValue(playlist.getNumWarmupCycles());
  numMeasurementCyclesSpinBox->setValue(playlist.getNumMeasurementCycles());
  storageDischargeCheckBox->setChecked(playlist.getStorageDischarge());
  restPeriodSpinBox->setValue(playlist.getRestPeriod());
  highVoltageCutoffSpinBox->setValue(playlist.getHighVoltageCutoff());
  lowVoltageCutoffSpinBox->setValue(playlist.getLowVoltageCutoff());
  chargeTemperatureCutoffSpinBox->setValue(playlist.getChargeTempCutoff());
  dischargeTemperatureCutoffSpinBox->setValue(
      playlist.getDischargeTempCutoff());
  chargeCurrentSafetyCutoffSpinBox->setValue(
      playlist.getChargeCurrentSafetyCutoff());
  dischargeCurrentSafetyCutoffSpinBox->setValue(
      playlist.getDischargeCurrentSafetyCutoff());
  prechargeRateSpinBox->setValue(playlist.getPrechargeRate());
  chargeRateSpinBox->setValue(playlist.getChargeRate());
  dischargeRateSpinBox->setValue(playlist.getDischargeRate());
  storageDischargeVoltageSpinBox->setValue(
      playlist.getStorageDischargeVoltage());
  acceptableCellImpedanceThresholdSpinBox->setValue(
      playlist.getAcceptableImpedanceThreshold());
}
