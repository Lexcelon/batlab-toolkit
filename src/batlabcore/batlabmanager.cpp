#include "batlabmanager.h"

BatlabManager::BatlabManager(QObject *parent) : QObject(parent) {
  qRegisterMetaType<QVector<uchar>>("QVector<uchar>");

  m_isCellPlaylistLoaded = false;

  QTimer *updateConnectedBatlabsTimer = new QTimer(this);
  connect(updateConnectedBatlabsTimer, &QTimer::timeout, this,
          &BatlabManager::updateConnectedBatlabs);
  updateConnectedBatlabsTimer->start(200);

  networkAccessManager = nullptr;
  QTimer::singleShot(100, this, &BatlabManager::initializeNetworkAccessManager);
  QTimer::singleShot(500, this,
                     &BatlabManager::requestAvailableFirmwareVersions);
}

void BatlabManager::startTests() {
  if (connectedBatlabsByPortName.size() == 0) {
    emit error(tr("At least one Batlab must be connected to start tests."));
    return;
  }

  for (auto batlab : connectedBatlabsByPortName.values()) {
    if (batlab->getFirmwareVersion() < MINIMUM_FIRMWARE_VERSION) {
      emit error(tr("All Batlabs must have firmware version of at least %1 in "
                    "order to start tests. Please upgrade Batlab firmware.")
                     .arg(MINIMUM_FIRMWARE_VERSION));
      return;
    }
    if (!batlab->getExternalPowerConnected()) {
      emit error(tr("All Batlabs must have external power connected in order "
                    "to start tests."));
      return;
    }
    for (int slot = 0; slot < 4; slot++) {
      if (batlab->getChannel(slot)->mode() != MODE_NO_CELL) {
        emit error(
            tr("Please start tests with all cells removed from the Batlabs. "
               "Cells will be placed in order as the tests progress."));
        return;
      }
    }
  }

  emit notify("Placing Cells",
              "When tests are in progress, you will need to place specific "
              "cells in their assigned Batlab channel.\n\n"
              "When a channel is free and you should place a cell there, this "
              "information will be visible in either the \"Batlabs\" or "
              "\"Results\" view.\n\n"
              "When a cell has been placed in its channel, the test will "
              "automatically start running on that cell.");

  // This will automatically start tests on those channels
  assignRemainingCellsToOpenChannels();

  emit testsInProgressState(true);
  // Trigger redraw of Batlabs to disable firmware upgrade button
  processUpdatedBatlabInfo();
}

void BatlabManager::stopTests() {
  abortAllTests();

  emit testsInProgressState(false);
  // Trigger redraw of Batlabs to enable firmware upgrade button
  processUpdatedBatlabInfo();
}

CellPlaylist BatlabManager::loadedPlaylist() { return m_loadedPlaylist; }

void BatlabManager::assignRemainingCellsToOpenChannels() {
  for (auto cellResult : m_cellResults.values()) {
    if (cellResult.batlabSerial == -1) {
      findBatlabForCell(cellResult);
    }
  }
  emit cellResultsUpdated(m_cellResults.values().toVector());
}

void BatlabManager::findBatlabForCell(cellResultsStatusInfo cell) {
  for (auto batlab : connectedBatlabsByPortName.values()) {
    for (int channel = 0; channel < 4; channel++) {
      if (batlab->getChannel(channel)->mode() == MODE_NO_CELL &&
          batlab->getChannel(channel)->info.cellName == "") {
        m_cellResults[cell.cellName].batlabSerial = batlab->getSerialNumber();
        m_cellResults[cell.cellName].channel = channel;
        batlab->getChannel(channel)->info.cellName = cell.cellName;
        batlab->getChannel(channel)->startTest();
        return;
      }
    }
  }
}

bool BatlabManager::hasPartialCellResults(CellPlaylist playlist) {
  QMap<QString, cellResultsStatusInfo> cellResults;
  QDir resultsDir(playlist.getPlaylistOutputDirectory());
  for (auto cellName : playlist.getCellNames()) {
    cellResults[cellName] = BatlabLib::createInitializedCellResultsStatusInfo();
    cellResults[cellName].cellName = cellName;
    // Note if that cell has some results
    QFile cellFile(resultsDir.absoluteFilePath(playlist.getCellPlaylistName() +
                                               "_" + cellName + ".csv"));
    if (cellFile.exists()) {
      cellResults[cellName].hasSomeResults = true;
    }
  }
  QFile summaryFile(
      resultsDir.absoluteFilePath(playlist.getCellPlaylistName() + ".csv"));
  if (summaryFile.exists()) {
    if (!summaryFile.open(QIODevice::ReadOnly)) {
      qWarning() << summaryFile.errorString();
      return true;
    }
    QByteArray output;
    QString line;
    while (!line.startsWith("Cell Name") &&
           !summaryFile.atEnd()) { // JSON and headers
      line = summaryFile.readLine();
      output.append(line);
    }
    while (!summaryFile.atEnd()) { // Data
      line = summaryFile.readLine();
      auto values = QString(line).remove('"').split(',');
      auto name = values[0];
      cellResults[name].hasSomeResults = true;
      if (values[10] == "SUMMARY") {
        cellResults[name].hasCompleteResults = true;
      }
    }
  }
  for (auto cell : cellResults.values()) {
    if (cell.hasSomeResults && !cell.hasCompleteResults) {
      return true;
    }
  }
  return false;
}

void BatlabManager::savePlaylist() {
  m_loadedPlaylist.write(m_loadedPlaylist.getPlaylistSaveFilename());
  emit cellPlaylistEditedState(false);
}

void BatlabManager::savePlaylistAs(QString filename) {
  m_loadedPlaylist.write(filename);
  m_loadedPlaylist.setPlaylistSaveFilename(filename);
  emit cellPlaylistEditedState(false);
}

// This will automatically archive incomplete cell results (move them to
// archive_* files). If you want to know in advance if that will happen, call
// hasIncompleteCellResults().
void BatlabManager::loadPlaylist(CellPlaylist playlist) {
  m_loadedPlaylist = playlist;

  m_cellResults.clear();
  QDir resultsDir(playlist.getPlaylistOutputDirectory());
  for (auto cellName : playlist.getCellNames()) {
    m_cellResults[cellName] =
        BatlabLib::createInitializedCellResultsStatusInfo();
    m_cellResults[cellName].cellName = cellName;
    // Note if that cell has some results
    QFile cellFile(resultsDir.absoluteFilePath(playlist.getCellPlaylistName() +
                                               "_" + cellName + ".csv"));
    if (cellFile.exists()) {
      m_cellResults[cellName].hasSomeResults = true;
    }
  }
  // Go through summary CSV and mark cells with some data there
  // Also mark cells that have complete SUMMARY entries, load those values
  QFile summaryFile(
      resultsDir.absoluteFilePath(playlist.getCellPlaylistName() + ".csv"));
  if (summaryFile.exists()) {
    if (!summaryFile.open(QIODevice::ReadOnly)) {
      qWarning() << summaryFile.errorString();
      return;
    }
    QByteArray output;
    QString line;
    while (!line.startsWith("Cell Name") &&
           !summaryFile.atEnd()) { // JSON and headers
      line = summaryFile.readLine();
      output.append(line);
    }
    while (!summaryFile.atEnd()) {
      line = summaryFile.readLine();
      auto values = QString(line).remove('"').split(',');
      if (values.length() < 27) {
        continue;
      }
      auto name = values[0];
      m_cellResults[name].hasSomeResults = true;
      if (values[10] == "SUMMARY") {
        m_cellResults[name].batlabSerial = values[1].toInt();
        m_cellResults[name].channel = values[2].toInt();
        m_cellResults[name].hasCompleteResults = true;
        m_cellResults[name].chargeCapacity = values[19].toFloat();
        m_cellResults[name].chargeCapacityRange = values[20].toFloat();
        m_cellResults[name].energyCapacity = values[21].toFloat();
        m_cellResults[name].energyCapacityRange = values[22].toFloat();
        m_cellResults[name].impedance = values[23].toFloat();
        m_cellResults[name].avgVoltage = values[24].toFloat();
        m_cellResults[name].avgCurrent = values[25].toFloat();
      }
    }
  }
  summaryFile.close();

  // Handle those cells with only partial information
  // First, rename individual cell files to archive_*
  for (auto cell : m_cellResults.values()) {
    if (cell.hasSomeResults && !cell.hasCompleteResults) {
      QString currentName = resultsDir.absoluteFilePath(
          playlist.getCellPlaylistName() + "_" + cell.cellName + ".csv");
      QString archiveName = resultsDir.absoluteFilePath(
          "archive_" + playlist.getCellPlaylistName() + "_" + cell.cellName +
          ".csv");
      QFile currentFile(currentName);
      QFile archiveFile(archiveName);
      if (archiveFile.exists()) {
        if (!currentFile.open(QIODevice::ReadOnly)) {
          qWarning() << currentFile.errorString();
          return;
        }
        QByteArray output;
        QString line;
        while (!line.startsWith("Cell Name") &&
               !currentFile.atEnd()) { // JSON and headers
          line = currentFile.readLine();
        }
        while (!currentFile.atEnd()) {
          line = currentFile.readLine();
          output.append(line);
        }
        if (!archiveFile.open(QIODevice::Append)) {
          qWarning() << archiveFile.errorString();
          return;
        }
        archiveFile.write(output);
        archiveFile.close();
        currentFile.remove();
      } else {
        QFile::rename(currentName, archiveName);
      }
    }
  }
  // Next, move lvl2 lines for those cells to their archive_ file
  if (summaryFile.exists()) {
    if (!summaryFile.open(QIODevice::ReadOnly)) {
      qWarning() << summaryFile.errorString();
      return;
    }
    QByteArray output;
    QString line;
    while (!line.startsWith("Cell Name") &&
           !summaryFile.atEnd()) { // JSON and headers
      line = summaryFile.readLine();
      output.append(line);
    }
    while (!summaryFile.atEnd()) { // Data
      line = summaryFile.readLine();
      auto values = QString(line).remove('"').split(',');
      auto name = values[0];
      auto cell = m_cellResults[name];
      if (cell.hasSomeResults && !cell.hasCompleteResults) {
        QFile cellFile(resultsDir.absoluteFilePath(
            "archive_" + playlist.getCellPlaylistName() + "_" + cell.cellName +
            ".csv"));
        if (!cellFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
          qWarning() << summaryFile.errorString();
          return;
        }
        cellFile.write(line.toUtf8());
        cellFile.close();
      } else {
        output.append(line);
      }
    }
    summaryFile.close();

    // Rewrite summary file without those partial lvl2 lines
    if (!summaryFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      qWarning() << summaryFile.errorString();
      return;
    }
    summaryFile.write(output);
    summaryFile.close();
  }

  // We have archived the partial results, so those cells don't have any results
  // now
  for (auto cellName : m_cellResults.keys()) {
    if (m_cellResults[cellName].hasSomeResults &&
        !m_cellResults[cellName].hasCompleteResults) {
      m_cellResults[cellName].hasSomeResults = false;
      m_cellResults[cellName].hasCompleteResults = false;
    }
  }

  // Record that we loaded the playlist
  m_isCellPlaylistLoaded = true;
  emit cellPlaylistLoaded(m_loadedPlaylist);

  // Load any existing results into GUI
  processCellResultsUpdated();
}

void BatlabManager::updatePlaylist(CellPlaylist playlist) {
  m_loadedPlaylist.setCellPlaylistName(playlist.getCellPlaylistName());
  m_loadedPlaylist.setCellChemistryType(playlist.getCellChemistryType());
  m_loadedPlaylist.setCellNames(playlist.getCellNames());
  m_loadedPlaylist.setNumWarmupCycles(playlist.getNumWarmupCycles());
  m_loadedPlaylist.setNumMeasurementCycles(playlist.getNumMeasurementCycles());
  m_loadedPlaylist.setStorageDischarge(playlist.getStorageDischarge());
  m_loadedPlaylist.setRestPeriod(playlist.getRestPeriod());
  m_loadedPlaylist.setHighVoltageCutoff(playlist.getHighVoltageCutoff());
  m_loadedPlaylist.setLowVoltageCutoff(playlist.getLowVoltageCutoff());
  m_loadedPlaylist.setChargeTempCutoff(playlist.getChargeTempCutoff());
  m_loadedPlaylist.setDischargeTempCutoff(playlist.getDischargeTempCutoff());
  m_loadedPlaylist.setChargeCurrentSafetyCutoff(
      playlist.getChargeCurrentSafetyCutoff());
  m_loadedPlaylist.setDischargeCurrentSafetyCutoff(
      playlist.getDischargeCurrentSafetyCutoff());
  m_loadedPlaylist.setPrechargeRate(playlist.getPrechargeRate());
  m_loadedPlaylist.setChargeRate(playlist.getChargeRate());
  m_loadedPlaylist.setDischargeRate(playlist.getDischargeRate());
  m_loadedPlaylist.setStorageDischargeVoltage(
      playlist.getStorageDischargeVoltage());
  m_loadedPlaylist.setAcceptableImpedanceThreshold(
      playlist.getAcceptableImpedanceThreshold());

  m_loadedPlaylist.setEnablePulse(playlist.getEnablePulse());
  m_loadedPlaylist.setPulseDischargeOffTime(
      playlist.getPulseDischargeOffTime());
  m_loadedPlaylist.setPulseDischargeOnTime(playlist.getPulseDischargeOnTime());
  m_loadedPlaylist.setPulseChargeOffTime(playlist.getPulseChargeOffTime());
  m_loadedPlaylist.setPulseChargeOnTime(playlist.getPulseChargeOnTime());
  m_loadedPlaylist.setPulseChargeOffRate(playlist.getPulseChargeOffRate());
  m_loadedPlaylist.setPulseDischargeOffRate(
      playlist.getPulseDischargeOffRate());

  m_loadedPlaylist.setEnableTrickle(playlist.getEnableTrickle());
  m_loadedPlaylist.setTrickleDischargeEngageLimit(
      playlist.getTrickleDischargeEngageLimit());
  m_loadedPlaylist.setTrickleChargeEngageLimit(
      playlist.getTrickleChargeEngageLimit());
  m_loadedPlaylist.setTrickleChargeRate(playlist.getTrickleChargeRate());
  m_loadedPlaylist.setTrickleDischargeRate(playlist.getTrickleDischargeRate());

  m_loadedPlaylist.setEnableConstantVoltage(
      (playlist.getEnableConstantVoltage()));
  m_loadedPlaylist.setConstantVoltageSensitivity(
      playlist.getConstantVoltageSensitivity());
  m_loadedPlaylist.setConstantVoltageStepSize(
      playlist.getConstantVoltageStepSize());

  emit cellPlaylistEditedState(true);
}

void BatlabManager::abortAllTests() {
  for (auto portName : connectedBatlabsByPortName.keys()) {
    connectedBatlabsByPortName[portName]->abortTests();
  }
}

void BatlabManager::initializeNetworkAccessManager() {
  networkAccessManager = new QNetworkAccessManager;
}

void BatlabManager::requestAvailableFirmwareVersions() {
  if (networkAccessManager != nullptr) {
    firmwareVersionsReply = networkAccessManager->get(
        QNetworkRequest(QUrl("https://api.github.com/repos/Lexcelon/"
                             "batlab-firmware-measure/releases")));
    connect(firmwareVersionsReply, &QNetworkReply::finished, this,
            &BatlabManager::processAvailableFirmwareVersions);
  }
}

void BatlabManager::processAvailableFirmwareVersions() {
  QJsonDocument jsonDoc =
      QJsonDocument::fromJson(firmwareVersionsReply->readAll());
  QJsonArray jsonArray = jsonDoc.array();
  for (int i = 0; i < jsonArray.size(); i++) {
    availableFirmwareVersionToUrl
        [jsonArray[i].toObject()["tag_name"].toString()] =
            jsonArray[i]
                .toObject()["assets"]
                .toArray()[0]
                .toObject()["browser_download_url"]
                .toString();
  }
}

void BatlabManager::updateConnectedBatlabs() {
  QList<QSerialPortInfo> availableCommPorts = QSerialPortInfo::availablePorts();
  QStringList availableCommPortNames;

  for (int i = 0; i < availableCommPorts.size(); i++) {
    // On Windows availableports() returns those that are not active, meaning
    // you have to check individually to see if they are active. Otherwise it
    // never thinks you unplug a Batlab.
    // https://stackoverflow.com/questions/24854597/qserialportinfo-returns-more-com-ports-than-i-have
    // https://bugreports.qt.io/browse/QTBUG-39748
    if (!availableCommPorts.at(i).description().isEmpty() &&
        availableCommPorts.at(i).vendorIdentifier() == 0x04D8 &&
        availableCommPorts.at(i).productIdentifier() == 0x000A) {
      availableCommPortNames.append(availableCommPorts.at(i).portName());
    }
  }

  // Check if Batlabs have been added
  for (int i = 0; i < availableCommPortNames.size(); i++) {
    QString portName = availableCommPortNames[i];
    if (!connectedBatlabsByPortName.contains(portName)) {
      addNewBatlab(portName);
    }
  }

  // Check if Batlabs have been removed
  for (int i = 0; i < connectedBatlabsByPortName.keys().size(); i++) {
    QString portName = connectedBatlabsByPortName.keys()[i];
    if (!availableCommPortNames.contains(portName)) {
      removeBatlab(portName);
    }
  }
}

void BatlabManager::addNewBatlab(QString portName) {
  Batlab *batlab = new Batlab(portName, this);
  connect(batlab, &Batlab::infoUpdated, this,
          &BatlabManager::processUpdatedBatlabInfo);
  connect(batlab, &Batlab::cellResultsUpdated, this,
          &BatlabManager::processCellResultsUpdated);
  connectedBatlabsByPortName[portName] = batlab;
  processUpdatedBatlabInfo();
}

void BatlabManager::processCellResultsUpdated() {
  for (auto batlab : connectedBatlabsByPortName.values()) {
    for (int i = 0; i < 4; i++) {
      auto channelInfo = batlab->getChannel(i)->info;
      if (channelInfo.cellName != "") {
        m_cellResults[channelInfo.cellName].testInProgress =
            channelInfo.testInProgress;
      }
    }
  }

  QVector<cellResultsStatusInfo> infos;
  for (auto cell : m_cellResults.values()) {
    infos.append(cell);
  }
  emit cellResultsUpdated(infos);

  assignRemainingCellsToOpenChannels();
}

void BatlabManager::removeBatlab(QString portName) {
  batlabSerialToFirmwareVersionWaiting.remove(
      connectedBatlabsByPortName[portName]->getSerialNumber());
  delete connectedBatlabsByPortName[portName];
  connectedBatlabsByPortName.remove(portName);
  processUpdatedBatlabInfo();
}

void BatlabManager::processUpdatedBatlabInfo() {
  QVector<batlabStatusInfo> infos;
  for (int i = 0; i < connectedBatlabsByPortName.keys().size(); i++) {
    QString portName = connectedBatlabsByPortName.keys()[i];
    // Only show information for devices that have received valid responses
    // (i.e. are Batlabs)
    if (connectedBatlabsByPortName[portName]->hasReceivedValidResponse()) {
      batlabStatusInfo info = connectedBatlabsByPortName[portName]->getInfo();
      infos.push_back(info);
    }
  }
  emit batlabInfoUpdated(infos, getLatestFirmwareVersion());
}

int BatlabManager::getLatestFirmwareVersion() {
  auto versions = getFirmwareVersions();
  QVector<int> versions_i;
  for (auto version : versions) {
    versions_i.append(qRound(version.remove(0, 1).toFloat()));
  }
  if (versions_i.size() > 0) {
    return *std::max_element(versions_i.begin(), versions_i.end());
  } else {
    return 0;
  }
}

QVector<batlabStatusInfo> BatlabManager::getBatlabInfos() {
  QVector<batlabStatusInfo> infos;
  for (int i = 0; i < connectedBatlabsByPortName.keys().size(); i++) {
    QString portName = connectedBatlabsByPortName.keys()[i];
    // Only return information for devices that have received valid responses
    // (i.e. are Batlabs)
    if (connectedBatlabsByPortName[portName]->hasReceivedValidResponse()) {
      batlabStatusInfo info = connectedBatlabsByPortName[portName]->getInfo();
      infos.push_back(info);
    }
  }
  return infos;
}

bool BatlabManager::testsInProgress() {
  for (auto batlab : connectedBatlabsByPortName.values()) {
    if (batlab->testsInProgress()) {
      return true;
    }
  }
  return false;
}

QVector<QString> BatlabManager::getFirmwareVersions() {
  return availableFirmwareVersionToUrl.keys().toVector();
}

void BatlabManager::processRegisterReadRequest(int serial, int ns,
                                               int address) {
  QString portName = getPortNameFromSerial(serial);
  if (!portName.isEmpty()) {
    connectedBatlabsByPortName[portName]->registerRead(ns, address);
  }
}

void BatlabManager::processRegisterWriteRequest(int serial, int ns, int address,
                                                int value) {
  QString portName = getPortNameFromSerial(serial);
  if (!portName.isEmpty()) {
    connectedBatlabsByPortName[portName]->registerWrite(ns, address, value);
  }
}

void BatlabManager::processFirmwareFlashRequest(int serial,
                                                QString firmwareVersion) {
  if (testsInProgress()) {
    qWarning() << "Cannot flash firmware while tests are in progress.";
    return;
  }

  if (batlabSerialToFirmwareVersionWaiting.keys().contains(serial)) {
    qWarning() << "Batlab " << QString::number(serial)
               << " already has pending firmware update.";
    return;
  }

  batlabSerialToFirmwareVersionWaiting[serial] = firmwareVersion;

  QString firmwareFileUrl = availableFirmwareVersionToUrl[firmwareVersion];
  QString firmwareFilename = firmwareVersion + ".bin";

  QDir appLocalDataPath(
      QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation)
          .first());
  QString firmwareDirPath =
      appLocalDataPath.absoluteFilePath("firmware-bin-files");

  QDir dir;
  if (!dir.mkpath(firmwareDirPath)) {
    qWarning() << "Unable to find/make firmware download path.";
  }

  QString firmwareFilePath =
      QDir(firmwareDirPath).absoluteFilePath(firmwareFilename);
  QFileInfo firmwareFileInfo(firmwareFilePath);

  // Check if file already exists and is correct size. If so, flash.
  if (firmwareFileInfo.exists() &&
      firmwareFileInfo.size() == FIRMWARE_FILE_SIZE) {
    qInfo() << tr(
        "Requested firmware version already downloaded, proceeding to flash.");
    batlabSerialToFirmwareVersionWaiting.remove(serial);
    connectedBatlabsByPortName[getPortNameFromSerial(serial)]
        ->initiateFirmwareFlash(firmwareFilePath);
    return;
  } else {
    if (networkAccessManager == nullptr) {
      qWarning() << "Cannot request firmware download, network access manager "
                    "has not been initialized.";
      return;
    }

    // Check if URL already in map
    // If yes, do nothing
    if (pendingFirmwareDownloadVersions.keys().contains(firmwareVersion)) {
      // This firmware version already has a download pending. No action.
      return;
    }
    // If no, create network reply and add to map
    else {
      qInfo() << tr("Downloading requested firmware version.");
      QNetworkRequest request =
          QNetworkRequest(QUrl(availableFirmwareVersionToUrl[firmwareVersion]));
      request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
      QNetworkReply *firmwareDownloadReply = networkAccessManager->get(request);
      pendingFirmwareDownloadVersions[firmwareVersion] = firmwareDownloadReply;

      connect(firmwareDownloadReply, &QNetworkReply::finished, this,
              &BatlabManager::processFirmwareDownloadFinished);
      connect(firmwareDownloadReply,
              QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
              this, &BatlabManager::processFirmwareDownloadError);
    }
  }
}

void BatlabManager::processFirmwareDownloadError() {
  QNetworkReply *firmwareDownloadReply =
      qobject_cast<QNetworkReply *>(QObject::sender());
  qWarning() << tr("Unable to download requested firmware version, error: %1")
                    .arg(firmwareDownloadReply->errorString());
}

void BatlabManager::processFirmwareDownloadFinished() {
  QNetworkReply *firmwareDownloadReply =
      qobject_cast<QNetworkReply *>(QObject::sender());

  // Find what firmware version this was
  QString firmwareVersion;
  for (auto version : pendingFirmwareDownloadVersions.keys()) {
    if (pendingFirmwareDownloadVersions[version] == firmwareDownloadReply) {
      firmwareVersion = version;
      pendingFirmwareDownloadVersions.remove(version);
    }
  }

  QString firmwareFileUrl = firmwareDownloadReply->url().toString();
  QString firmwareFilename = firmwareVersion + ".bin";

  QDir appLocalDataPath(
      QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation)
          .first());
  QString firmwareDirPath =
      appLocalDataPath.absoluteFilePath("firmware-bin-files");

  QDir dir;
  if (!dir.mkpath(firmwareDirPath)) {
    qWarning() << "Unable to find/make firmware download path.";
  }

  QString firmwareFilePath =
      QDir(firmwareDirPath).absoluteFilePath(firmwareFilename);
  QFile file(firmwareFilePath);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Could not open firmware file for write.";
    return;
  }
  const QByteArray data = firmwareDownloadReply->readAll();
  qInfo() << tr("Saving downloaded firmware.");
  file.write(data);
  file.close();

  // For each Batlab wanting this, flash it
  for (auto serial : batlabSerialToFirmwareVersionWaiting.keys()) {
    if (batlabSerialToFirmwareVersionWaiting[serial] == firmwareVersion) {
      QString portName = getPortNameFromSerial(serial);
      batlabSerialToFirmwareVersionWaiting.remove(serial);
      if (!portName.isEmpty()) {
        connectedBatlabsByPortName[portName]->initiateFirmwareFlash(
            firmwareFilePath);
      }
    }
  }
}

QString BatlabManager::getPortNameFromSerial(int serial) {
  for (int i = 0; i < connectedBatlabsByPortName.size(); i++) {
    QString portName = connectedBatlabsByPortName.keys()[i];
    batlabStatusInfo info = connectedBatlabsByPortName[portName]->getInfo();
    if (info.serialNumberComplete == serial) {
      return portName;
    }
  }
  qWarning() << "No Batlab with serial" << serial << "was found.";
  return "";
}
