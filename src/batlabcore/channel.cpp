#include "batlab.h"
#include "batlabmanager.h"

Channel::Channel(int slot, QObject *parent) : QObject(parent) {
  info.slot = slot;

  info.cellName = "";
  info.testInProgress = false;
  info.preChargeComplete = false;
  info.preChargeError = false;
  info.numWarmupCycles = -1;
  info.numWarmupCyclesCompleted = -1;
  info.warmupCyclesError = false;
  info.numMeasurementCycles = -1;
  info.numMeasurementCyclesCompleted = -1;
  info.measurementCyclesError = false;
  info.storageDischarge = false;
  info.storageDischargeComplete = false;
  info.storageDischargeError = false;
  info.tempCalibB = -1;
  info.tempCalibR = -1;

  info.mode = MODE_NO_CELL;
  m_test_state = TS_IDLE;

  m_voltage_count = 0;
  m_voltage_avg = 0;
  info.voltage_prev = 0;
  m_voltage_error_count = 0;

  m_z_avg = 0;
  m_z_count = 0;

  m_current_count = 0;
  m_current_avg = 0;
  info.current_prev = 0;
  m_current_setpoint = 256;

  m_vcc = 5.0;

  m_e = 0;
  m_q = 0;

  m_temperature0 = -1;

  m_timeout_time = std::chrono::seconds(0);

  QTimer *getStartedTimer = new QTimer(this);
  getStartedTimer->setSingleShot(true);
  connect(getStartedTimer, &QTimer::timeout, this, &Channel::startTimer);
  getStartedTimer->start((info.slot + 1) * 100);
  // Have the channels not start at the exact same
  // time to try to stagger communication
}

void Channel::startTimer() {
  m_channelPeriodicCheckTimer = new QTimer(this);
  connect(m_channelPeriodicCheckTimer, &QTimer::timeout, this,
          &Channel::periodicCheck);
  m_channelPeriodicCheckTimer->start(
      static_cast<int>(REPORTING_PERIOD_DEFAULT * 1000));
}

void Channel::handleSerialResponseBundleReady(batlabPacketBundle bundle) {
  if (bundle.callback == "handleChannelPeriodicCheckResponse") {
    handlePeriodicCheckResponse(bundle.packets);
  } else if (bundle.callback == "handleCurrentCompensateResponse") {
    handleCurrentCompensateResponse();
  } else if (bundle.callback == "handleStartTestResponse") {
    handleStartTestResponse(bundle.packets);
  } else if (bundle.callback == "handleImpedanceResponse") {
    handleImpedanceResponse(bundle.packets);
  } else if (bundle.callback == "handleStateMachineResponse") {
    // No need to do anything
  } else if (bundle.callback == "handleSetModeResponse") {
    handleSetModeResponse(bundle.packets);
  } else if (bundle.callback == "handleLogLvl2Response") {
    handleLogLvl2Response(bundle.packets);
  } else if (bundle.callback == "handleAbortTestResponse") {
    // No need to do anything
  } else {
    qWarning() << tr("%1 not implemented in Channel implementation")
                      .arg(bundle.callback);
  }
}

Batlab *Channel::batlab() { return dynamic_cast<Batlab *>(parent()); }

CellPlaylist Channel::playlist() { return batlab()->playlist(); }

void Channel::startTest() {
  m_channelPeriodicCheckTimer->setInterval(
      static_cast<int>(playlist().getReportingPeriod() * 1000));

  QVector<BatlabPacket> startTestPackets;

  startTestPackets.append(
      BatlabPacket(info.slot, VOLTAGE_LIMIT_CHG,
                   Encoder(playlist().getHighVoltageCutoff()).asVoltage()));
  startTestPackets.append(
      BatlabPacket(info.slot, VOLTAGE_LIMIT_DCHG,
                   Encoder(playlist().getLowVoltageCutoff()).asVoltage()));
  startTestPackets.append(BatlabPacket(
      info.slot, CURRENT_LIMIT_CHG,
      Encoder(playlist().getChargeCurrentSafetyCutoff()).asCurrent()));
  startTestPackets.append(BatlabPacket(
      info.slot, CURRENT_LIMIT_DCHG,
      Encoder(playlist().getDischargeCurrentSafetyCutoff()).asCurrent()));
  startTestPackets.append(
      BatlabPacket(info.slot, TEMP_LIMIT_CHG,
                   Encoder(playlist().getChargeTempCutoff())
                       .asTemperatureC(info.tempCalibR, info.tempCalibB)));
  startTestPackets.append(
      BatlabPacket(info.slot, TEMP_LIMIT_DCHG,
                   Encoder(playlist().getDischargeTempCutoff())
                       .asTemperatureC(info.tempCalibR, info.tempCalibB)));
  startTestPackets.append(BatlabPacket(
      info.slot, CHARGEH,
      0)); // Only need to write to one of the charge registers to clear them

  if (playlist().getEnableConstantVoltage()) {
    // If we're doing constant voltage charging, we need to have current
    // resolution down to the small range
    startTestPackets.append(BatlabPacket(
        batlabNamespaces::UNIT, ZERO_AMP_THRESH, Encoder(0.05).asCurrent()));
  }

  startTestPackets.append(BatlabPacket(info.slot, TEMPERATURE));

  m_test_state = TS_PRECHARGE;
  info.numWarmupCycles = playlist().getNumWarmupCycles();
  info.numWarmupCyclesCompleted = 0;
  info.numMeasurementCycles = playlist().getNumMeasurementCycles();
  info.numMeasurementCyclesCompleted = 0;
  info.storageDischarge = playlist().getStorageDischarge();

  batlabPacketBundle packetBundle;
  packetBundle.packets = startTestPackets;
  packetBundle.callback = "handleStartTestResponse";
  packetBundle.channel = info.slot;
  batlab()->sendPacketBundle(packetBundle);
}

void Channel::stateMachine() {
  QVector<BatlabPacket> packets;
  if (m_test_state == TS_PRECHARGE && info.cellName != "") {
    if (info.mode == MODE_IDLE) {
      info.testInProgress = true;
      packets.append(BatlabPacket(info.slot, CURRENT_SETPOINT, 0));
      packets.append(BatlabPacket(info.slot, MODE, MODE_CHARGE)
                         .setSleepAfterTransaction_ms(10));
      packets.append(
          BatlabPacket(info.slot, CURRENT_SETPOINT,
                       Encoder(playlist().getPrechargeRate()).asSetpoint()));
    }
    // Handle feature to trickle charge the cell if close to voltage limit
    if (playlist().getEnableTrickle() &&
        !playlist().getEnableConstantVoltage()) {
      if (!m_trickle_engaged &&
          info.voltage_prev >
              static_cast<float>(playlist().getTrickleChargeEngageLimit())) {
        packets.append(BatlabPacket(
            info.slot, CURRENT_SETPOINT,
            Encoder(playlist().getTrickleChargeRate()).asSetpoint()));
        m_trickle_engaged = true;
      }
    }
    // Handle constant voltage charge
    else if (playlist().getEnableConstantVoltage()) {
      float std_impedance;
      if (m_z_avg > 0.5f) {
        std_impedance = 0.5f / 128.0f;
      } else if (m_z_avg < 0.01f) {
        std_impedance = 0.01f / 128.0f;
      } else {
        std_impedance = m_z_avg / 128.0f;
      }
      std_impedance =
          std_impedance *
          static_cast<float>(playlist().getConstantVoltageSensitivity());
      // If voltage is getting close to the cutoff point and current is flowing
      // at greater than a trickle
      if (info.voltage_prev >
              (static_cast<float>(playlist().getHighVoltageCutoff()) -
               (static_cast<float>(m_current_setpoint) * std_impedance)) &&
          m_current_setpoint > playlist().getConstantVoltageStepSize()) {
        // Scale down by 1/32th (default) of an amp
        packets.append(BatlabPacket(
            info.slot, CURRENT_SETPOINT,
            static_cast<quint16>(m_current_setpoint -
                                 playlist().getConstantVoltageStepSize())));
      }
    }

    if (info.mode == MODE_STOPPED) {
      info.preChargeComplete = true;
      logLvl2("PRECHARGE");
      m_test_state = TS_CHARGEREST;
      m_rest_time = std::chrono::system_clock::now();
      // We should rarely hit this condition - it means you don't want to make
      // any testing cycles, just charge up and stop, or charge up and equalize
      if (m_current_cycle >= (playlist().getNumWarmupCycles() +
                              playlist().getNumMeasurementCycles())) {
        if (playlist().getStorageDischarge()) {
          m_test_state = TS_POSTDISCHARGE;
          packets.append(BatlabPacket(
              info.slot, CURRENT_SETPOINT,
              Encoder(playlist().getDischargeRate()).asSetpoint()));
          packets.append(BatlabPacket(info.slot, MODE, MODE_DISCHARGE)
                             .setSleepAfterTransaction_ms(10));
        } else {
          m_test_state = TS_IDLE;
          completeTest();
        }
      }
    }
  } else if (m_test_state == TS_CHARGEREST && info.cellName != "") {
    std::chrono::time_point<std::chrono::system_clock> now =
        std::chrono::system_clock::now();
    std::chrono::duration<double> difference = now - m_rest_time;
    if (difference.count() > playlist().getRestPeriod()) {
      logLvl2("CHARGEREST");
      m_test_state = TS_DISCHARGE;

      // Reset pulse discharge variables
      m_pulse_state = true;
      m_pulse_discharge_on_time = std::chrono::system_clock::now();
      m_pulse_discharge_off_time = std::chrono::system_clock::now();
      m_trickle_engaged = false;

      packets.append(
          BatlabPacket(info.slot, CURRENT_SETPOINT,
                       Encoder(playlist().getDischargeRate()).asSetpoint()));
      packets.append(BatlabPacket(info.slot, MODE, MODE_DISCHARGE)
                         .setSleepAfterTransaction_ms(10));
      m_current_cycle++;
      info.numWarmupCyclesCompleted =
          std::min(m_current_cycle, info.numWarmupCycles);
      info.numMeasurementCyclesCompleted =
          std::max(0, m_current_cycle - info.numWarmupCycles);
    }
  } else if (m_test_state == TS_DISCHARGE && info.cellName != "") {
    // Handle feature to end test after certain amount of time
    if (m_timeout_time != std::chrono::seconds(0)) {
      std::chrono::time_point<std::chrono::system_clock> now =
          std::chrono::system_clock::now();
      std::chrono::duration<double> difference = now - m_start_time;
      if (difference.count() > m_timeout_time.count()) {
        abortTest();
      }
    }
    // Handle feature to pulse discharge the cell
    if (playlist().getEnablePulse()) {
      if (m_pulse_state) {
        if (m_pulse_discharge_on_time ==
            std::chrono::system_clock::from_time_t(0)) {
          m_pulse_discharge_on_time = std::chrono::system_clock::now();
        }
        std::chrono::time_point<std::chrono::system_clock> now =
            std::chrono::system_clock::now();
        std::chrono::duration<double> difference =
            now - m_pulse_discharge_on_time;
        if (difference.count() > playlist().getPulseDischargeOnTime() &&
            playlist().getPulseDischargeOnTime() > 0) {
          packets.append(BatlabPacket(
              info.slot, CURRENT_SETPOINT,
              Encoder(playlist().getPulseDischargeOffRate()).asSetpoint()));
          m_pulse_state = false;
          m_pulse_discharge_off_time = std::chrono::system_clock::now();
        }
      } else {
        if (m_pulse_discharge_off_time ==
            std::chrono::system_clock::from_time_t(0)) {
          m_pulse_discharge_off_time = std::chrono::system_clock::now();
        }
        std::chrono::time_point<std::chrono::system_clock> now =
            std::chrono::system_clock::now();
        std::chrono::duration<double> difference =
            now - m_pulse_discharge_off_time;
        if (difference.count() > playlist().getPulseDischargeOffTime() &&
            playlist().getPulseDischargeOffTime() > 0) {
          if (m_trickle_engaged) {
            packets.append(BatlabPacket(
                info.slot, CURRENT_SETPOINT,
                Encoder(playlist().getTrickleDischargeRate()).asSetpoint()));
          } else {
            packets.append(BatlabPacket(
                info.slot, CURRENT_SETPOINT,
                Encoder(playlist().getDischargeRate()).asSetpoint()));
          }
          m_pulse_state = true;
          m_pulse_discharge_on_time = std::chrono::system_clock::now();
        }
      }
    } else if (playlist().getEnableConstantVoltage()) {
      float std_impedance;
      if (m_z_avg > 0.5f) {
        std_impedance = 0.5f / 128.0f;
      } else if (m_z_avg < 0.01f) {
        std_impedance = 0.01f / 128.0f;
      } else {
        std_impedance = m_z_avg / 128.0f;
      }
      std_impedance =
          std_impedance *
          static_cast<float>(playlist().getConstantVoltageSensitivity());
      if (info.voltage_prev <
              (static_cast<float>(playlist().getLowVoltageCutoff()) +
               (static_cast<float>(m_current_setpoint) * std_impedance)) &&
          m_current_setpoint > playlist().getConstantVoltageStepSize()) {
        packets.append(BatlabPacket(
            info.slot, CURRENT_SETPOINT,
            static_cast<quint16>(m_current_setpoint -
                                 playlist().getConstantVoltageStepSize())));
      }
    }
    if (playlist().getEnableTrickle() &&
        !playlist().getEnableConstantVoltage()) {
      if (info.voltage_prev <
              static_cast<float>(playlist().getTrickleDischargeEngageLimit()) &&
          !m_trickle_engaged) {
        packets.append(BatlabPacket(
            info.slot, CURRENT_SETPOINT,
            Encoder(playlist().getTrickleDischargeRate()).asSetpoint()));
        m_trickle_engaged = true;
      }
    }
    if (info.mode == MODE_STOPPED) {
      logLvl2("DISCHARGE");
      m_test_state = TS_DISCHARGEREST;
      m_rest_time = std::chrono::system_clock::now();
    }
  } else if (m_test_state == TS_DISCHARGEREST) {
    std::chrono::time_point<std::chrono::system_clock> now =
        std::chrono::system_clock::now();
    std::chrono::duration<double> difference = now - m_rest_time;
    if (difference.count() > playlist().getRestPeriod()) {
      logLvl2("DISCHARGEREST");
      m_test_state = TS_CHARGE;

      // Reset pulse discharge variables
      m_pulse_state = true;
      m_pulse_charge_on_time = std::chrono::system_clock::now();
      m_pulse_charge_off_time = std::chrono::system_clock::now();
      m_trickle_engaged = false;

      packets.append(BatlabPacket(info.slot, CURRENT_SETPOINT, 0));
      packets.append(BatlabPacket(info.slot, MODE, MODE_CHARGE)
                         .setSleepAfterTransaction_ms(10));
      packets.append(
          BatlabPacket(info.slot, CURRENT_SETPOINT,
                       Encoder(playlist().getChargeRate()).asSetpoint()));
    }
  } else if (m_test_state == TS_CHARGE) {
    // Handle feature to pulse charge the cell
    if (playlist().getEnablePulse()) {
      if (m_pulse_state) {
        if (m_pulse_charge_on_time ==
            std::chrono::system_clock::from_time_t(0)) {
          m_pulse_charge_on_time = std::chrono::system_clock::now();
        }
        std::chrono::time_point<std::chrono::system_clock> now =
            std::chrono::system_clock::now();
        std::chrono::duration<double> difference = now - m_pulse_charge_on_time;
        if (difference.count() > playlist().getPulseChargeOnTime() &&
            playlist().getPulseChargeOnTime() > 0) {
          packets.append(BatlabPacket(
              info.slot, CURRENT_SETPOINT,
              Encoder(playlist().getPulseChargeOffRate()).asSetpoint()));
          m_pulse_state = false;
          m_pulse_charge_off_time = std::chrono::system_clock::now();
        }
      } else {
        if (m_pulse_charge_off_time ==
            std::chrono::system_clock::from_time_t(0)) {
          m_pulse_charge_off_time = std::chrono::system_clock::now();
        }
        std::chrono::time_point<std::chrono::system_clock> now =
            std::chrono::system_clock::now();
        std::chrono::duration<double> difference =
            now - m_pulse_charge_off_time;
        if (difference.count() > playlist().getPulseChargeOffTime() &&
            playlist().getPulseChargeOffTime() > 0) {
          if (m_trickle_engaged) {
            packets.append(BatlabPacket(
                info.slot, CURRENT_SETPOINT,
                Encoder(playlist().getTrickleChargeRate()).asSetpoint()));
          } else {
            packets.append(
                BatlabPacket(info.slot, CURRENT_SETPOINT,
                             Encoder(playlist().getChargeRate()).asSetpoint()));
          }
          m_pulse_state = true;
          m_pulse_charge_on_time = std::chrono::system_clock::now();
        }
      }
    } else if (playlist().getEnableConstantVoltage()) {
      float std_impedance;
      if (m_z_avg > 0.5f) {
        std_impedance = 0.5f / 128.0f;
      } else if (m_z_avg < 0.01f) {
        std_impedance = 0.01f / 128.0f;
      } else {
        std_impedance = m_z_avg / 128.0f;
      }
      std_impedance =
          std_impedance *
          static_cast<float>(playlist().getConstantVoltageSensitivity());
      if (info.voltage_prev >
              (static_cast<float>(playlist().getHighVoltageCutoff()) -
               (static_cast<float>(m_current_setpoint) * std_impedance)) &&
          m_current_setpoint > playlist().getConstantVoltageStepSize()) {
        packets.append(BatlabPacket(
            info.slot, CURRENT_SETPOINT,
            static_cast<quint16>(m_current_setpoint -
                                 playlist().getConstantVoltageStepSize())));
      }
    }
    if (playlist().getEnableTrickle() &&
        !playlist().getEnableConstantVoltage()) {
      if (info.voltage_prev >
              static_cast<float>(playlist().getTrickleChargeEngageLimit()) &&
          !m_trickle_engaged) {
        packets.append(BatlabPacket(
            info.slot, CURRENT_SETPOINT,
            Encoder(playlist().getTrickleChargeRate()).asSetpoint()));
        m_trickle_engaged = true;
      }
    }
    if (info.mode == MODE_STOPPED) {
      logLvl2("CHARGE");
      m_test_state = TS_CHARGEREST;
      m_rest_time = std::chrono::system_clock::now();
      if (m_current_cycle >= (playlist().getNumWarmupCycles() +
                              playlist().getNumMeasurementCycles())) {
        if (playlist().getStorageDischarge()) {
          m_test_state = TS_POSTDISCHARGE;
          packets.append(BatlabPacket(
              info.slot, CURRENT_SETPOINT,
              Encoder(playlist().getDischargeRate()).asSetpoint()));
          packets.append(BatlabPacket(info.slot, MODE, MODE_DISCHARGE)
                             .setSleepAfterTransaction_ms(10));
        } else {
          m_test_state = TS_IDLE;
          completeTest();
        }
      }
    }
  } else if (m_test_state == TS_POSTDISCHARGE) {
    if (info.mode == MODE_STOPPED ||
        info.voltage_prev <
            static_cast<float>(playlist().getStorageDischargeVoltage())) {
      logLvl2("POSTDISCHARGE");
      packets.append(BatlabPacket(info.slot, MODE, MODE_IDLE)
                         .setSleepAfterTransaction_ms(10));
      m_test_state = TS_IDLE;
      completeTest();
    }
  } else {
    qWarning() << tr("Test state %1 not implemented").arg(m_test_state);
  }
  batlabPacketBundle packetBundle;
  packetBundle.packets = packets;
  packetBundle.callback = "handleStateMachineResponse";
  packetBundle.channel = info.slot;
  batlab()->sendPacketBundle(packetBundle);
  emit resultsUpdated();
}

void Channel::handleStartTestResponse(QVector<BatlabPacket> response) {
  // Initialize the control variables
  m_start_time = std::chrono::system_clock::now();
  m_last_lvl1_time = std::chrono::system_clock::now();
  m_last_lvl2_time = std::chrono::system_clock::now();
  m_last_impedance_time = std::chrono::system_clock::now();
  m_rest_time = std::chrono::system_clock::now();

  m_voltage_avg = 0;
  m_voltage_count = 0;
  m_voltage_error_count = 0;
  info.voltage_prev = 0;

  m_z_avg = 0;
  m_z_count = 0;

  m_current_avg = 0;
  m_current_count = 0;
  info.current_prev = 0;

  int responseCounter = 0;
  while (response[responseCounter].getAddress() != TEMPERATURE &&
         response[responseCounter].getNamespace() != info.slot) {
    responseCounter++;
  }
  m_temperature0 = response[responseCounter].asTemperatureC(info.tempCalibR,
                                                            info.tempCalibB);
  m_delta_t = 0;

  m_q = 0;
  m_e = 0;

  m_vcc = 5.0;

  m_current_cycle = -1;

  // Control variables for pulse discharge test
  m_pulse_charge_on_time = std::chrono::system_clock::from_time_t(0);
  m_pulse_charge_off_time = std::chrono::system_clock::from_time_t(0);
  m_pulse_discharge_on_time = std::chrono::system_clock::from_time_t(0);
  m_pulse_discharge_off_time = std::chrono::system_clock::from_time_t(0);
  m_pulse_state = true;

  // Control variables for trickle charge/discharge at voltage limits
  m_trickle_engaged = false;

  emit resultsUpdated();
}

void Channel::currentCompensate(quint16 op_raw, quint16 sp_raw) {
  m_sp_raw = sp_raw;
  QVector<BatlabPacket> packets;
  packets.append(BatlabPacket(info.slot, CURRENT_SETPOINT, op_raw));
  batlabPacketBundle packetBundle;
  packetBundle.packets = packets;
  packetBundle.callback = "handleCurrentCompensateResponse";
  packetBundle.channel = info.slot;
  batlab()->sendPacketBundle(packetBundle);
}

void Channel::handleCurrentCompensateResponse() {
  m_current_setpoint = m_sp_raw;
}

void Channel::handlePeriodicCheckResponse(QVector<BatlabPacket> response) {
  int responseCounter = 0;

  info.mode = static_cast<ChannelMode>(response[responseCounter++].getValue());
  auto p = response[responseCounter++];

  if (response.length() == 2) {
    return;
  }

  float voltage = response[responseCounter++].asVoltage();
  float current = response[responseCounter++].asCurrent();
  float temperature = response[responseCounter++].asTemperatureC(
      info.tempCalibR, info.tempCalibB);

  // Patch for current compensation problem in firmware versions <= 3.
  // Fix is to move the current compensation control loop to software and turn
  // it off in hardware.
  auto op_raw = p.getValue();       // Actual operating point
  auto sp_raw = m_current_setpoint; // Current setpoint
  auto sp = sp_raw / 128.0;
  if (info.mode == MODE_CHARGE || info.mode == MODE_DISCHARGE) {
    if (current > 0 && (sp >= 0.35 || current < 0.37f)) {
      if (current < (static_cast<float>(sp) - 0.01f)) {
        op_raw++;
      } else if (current > (static_cast<float>(sp) + 0.01f)) {
        op_raw--;
      }
    }
    if (current > 4.02f) {
      op_raw--;
    }
    if (sp > 4.5) {
      op_raw = 575;
    }
    if (op_raw < playlist().getConstantVoltageStepSize() &&
        sp_raw > 0) { // Make sure that some amount of trickle current is
                      // flowing even if our setpoint is close to 0
      op_raw = static_cast<quint16>(playlist().getConstantVoltageStepSize());
    }
    if (op_raw > 575 &&
        sp_raw <= 575) { // If for some reason we read a garbage op_raw, then
                         // don't make that our new setpoint
      op_raw = sp_raw;
    }
    currentCompensate(op_raw, sp_raw);
  }

  float charge;
  auto set = response[responseCounter++].getValue();
  float multiplier = 6.0;
  if (!(set & SET_CH0_HI_RES)) {
    multiplier = 1.0;
  }
  auto chargeh1 = response[responseCounter++].getValue();
  auto chargel1 = response[responseCounter++].getValue();
  auto chargeh2 = response[responseCounter++].getValue();
  auto chargel2 = response[responseCounter++].getValue();
  if (std::isnan(static_cast<float>(chargeh1)) ||
      std::isnan(static_cast<float>(chargel1)) ||
      std::isnan(static_cast<float>(chargeh2))) {
    charge = NAN;
  } else if (chargeh2 == chargeh1) {
    auto data = (chargeh1 << 16) + chargel1;
    charge = ((multiplier * data / powf(2, 15)) * 4.096f / 9.765625f);
  } else {
    auto data = (chargeh2 << 16) + chargel2;
    charge = ((multiplier * data / powf(2, 15)) * 4.096f / 9.765625f);
  }

  m_last_charge = charge;

  responseCounter++; // Skip duty (unused)
  auto vc = response[responseCounter++].asVcc();

  if (!std::isnan(static_cast<float>(vc))) {
    if (vc < 4.35f) {
      qWarning() << tr("VCC on Batlab %1 is dangerously low - consider using "
                       "more robust powered hub")
                        .arg(batlab()->getSerialNumber());
    }
    if (vc < 4.1f && m_vcc < 4.1f) {
      abortTest();
      qWarning() << tr("Test aborted due to low VCC: Batlab %1, Channel %2")
                        .arg(batlab()->getSerialNumber())
                        .arg(info.slot);
    }
    m_vcc = vc;
  }

  // Detect voltage measurement inconsistency hardware problem that was found
  // on a couple of batlabs
  if (!std::isnan(voltage) && !std::isnan(current)) {
    if (info.current_prev > 0.05f && info.voltage_prev > 0.5f) {
      if (std::abs(current - info.current_prev) < 0.05f) {
        if (info.voltage_prev - voltage > 0.2f) {
          m_voltage_error_count++;
          qWarning() << tr("Unexpected voltage jump detected on Batlab %1, "
                           "Channel %2")
                            .arg(batlab()->getSerialNumber())
                            .arg(info.slot);
          if (m_voltage_error_count > 5) {
            abortTest();
            qWarning()
                << tr("Test aborted due to voltage measurement "
                      "inconsistency. "
                      "Possible hardware problem with: Batlab %1, Channel %2")
                       .arg(batlab()->getSerialNumber())
                       .arg(info.slot);
          }
        }
      }
    }
    info.voltage_prev = voltage;
    info.current_prev = current;
    m_voltage_avg =
        m_voltage_avg + (voltage - m_voltage_avg) / ++m_voltage_count;
    m_current_avg =
        m_current_avg + (current - m_current_avg) / ++m_current_count;
    m_e = charge * m_voltage_avg;
  }

  if (m_temperature0 < 0) {
    m_temperature0 = temperature;
  }
  m_delta_t = temperature - m_temperature0;

  if (std::chrono::duration_cast<std::chrono::seconds>(m_ts - m_last_lvl1_time)
          .count() > playlist().getReportingPeriod()) {
    m_last_lvl1_time = std::chrono::system_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(m_ts -
                                                         m_last_impedance_time)
                .count() > playlist().getImpedanceReportingPeriod() &&
        playlist().getImpedanceReportingPeriod() > 0 && !m_trickle_engaged) {
    }
    //    impedance();  // TODO enable this
  }
  if (info.mode != MODE_NO_CELL && info.mode != MODE_BACKWARDS) {
    QString logstr = "";
    logstr += info.cellName + "," +
              QString::number(batlab()->getSerialNumber()) + ",";
    logstr += QString::number(info.slot) + ",";
    logstr += QDateTime::fromTime_t(
                  static_cast<uint>(std::chrono::system_clock::to_time_t(m_ts)))
                  .toString("MM/dd/yyyy hh:mm:ss AP") +
              ",";
    logstr += QString::number(static_cast<double>(voltage), 'f', 4) + ",";
    logstr += QString::number(static_cast<double>(current), 'f', 4) + ",";
    logstr += QString::number(static_cast<double>(temperature), 'f', 4) + ",,";
    logstr += QString::number(static_cast<double>(m_e), 'f', 4) + ",";
    logstr += QString::number(static_cast<double>(charge), 'f', 4) + ",";
    logstr += L_TEST_STATE[m_test_state] + ",,,,,,,,";
    logstr += QString::number(static_cast<double>(m_vcc), 'f', 4) + ",,,,\n";
    logLvl1(logstr);
  }

  // Run the test state machine - decides what to do next
  stateMachine();
}

void Channel::logLvl1(QString logstr) {
  // Don't log unless we are in a measurement cycle
  if (m_current_cycle <= playlist().getNumWarmupCycles()) {
    return;
  }
  QDir resultsDir(playlist().getPlaylistOutputDirectory());
  QFile cellFile(resultsDir.absoluteFilePath(playlist().getCellPlaylistName() +
                                             "_" + info.cellName + ".csv"));
  if (!resultsDir.exists()) {
    resultsDir.mkpath(resultsDir.path());
  }
  if (!cellFile.exists()) {
    if (!cellFile.open(QIODevice::WriteOnly)) {
      qWarning() << "Unable to create cell results file";
      return;
    }
    cellFile.write(QString("\"" + playlist().toJson().replace("\"", "\"\"") +
                           "\",,,,,,,,,,,,,,,,,,,,,,,,,\n")
                       .toUtf8());
    cellFile.write(
        "Cell Name,Batlab Serial,Channel,Timestamp (s),Voltage (V),Current "
        "(A),Temperature (C),Impedance (Ohm),Energy (J),Charge (Coulombs),Test "
        "State,Charge Capacity (Coulombs),Energy Capacity (J),Avg "
        "Impedance (Ohm),delta Temperature (C),Average Current (A),Average "
        "Voltage,Runtime (s),VCC "
        "(V),Charge Capacity,Charge Capacity Range,Energy Capacity,Energy "
        "Capacity Range,Impedance,Average Voltage,"
        "Average Current\n");
    cellFile.close();
  }
  if (!cellFile.open(QIODevice::Append)) {
    qWarning() << "Unable to open cell results file";
    return;
  }
  cellFile.write(logstr.toUtf8());
}

void Channel::logLvl2(QString type) {
  // Don't log unless we are in a measurement cycle
  if (m_current_cycle <= playlist().getNumWarmupCycles()) {
    return;
  }
  QDir resultsDir(playlist().getPlaylistOutputDirectory());
  QFile summaryFile(
      resultsDir.absoluteFilePath(playlist().getCellPlaylistName() + ".csv"));
  if (!resultsDir.exists()) {
    resultsDir.mkpath(resultsDir.path());
  }
  if (!summaryFile.exists()) {
    if (!summaryFile.open(QIODevice::WriteOnly)) {
      qWarning() << "Unable to create summary results file";
      return;
    }
    summaryFile.write(QString("\"" + playlist().toJson().replace("\"", "\"\"") +
                              "\",,,,,,,,,,,,,,,,,,,,,,,,,\n")
                          .toUtf8());
    summaryFile.write(
        "Cell Name,Batlab Serial,Channel,Timestamp (s),Voltage (V),Current "
        "(A),Temperature (C),Impedance (Ohm),Energy (J),Charge (Coulombs),Test "
        "State,Charge Capacity (Coulombs),Energy Capacity (J),Avg "
        "Impedance (Ohm),delta Temperature (C),Average Current (A),Average "
        "Voltage,Runtime (s),VCC "
        "(V),Charge Capacity,Charge Capacity Range,Energy Capacity,Energy "
        "Capacity Range,Impedance,Average Voltage,"
        "Average Current\n");
    summaryFile.close();
  }
  if (!summaryFile.open(QIODevice::Append)) {
    qWarning() << "Unable to open summary results file";
    return;
  }

  QString state = L_TEST_STATE[m_test_state];
  std::chrono::duration<double> runtime =
      std::chrono::system_clock::now() - m_last_lvl2_time;
  m_last_lvl2_time = std::chrono::system_clock::now();

  QString logstr = "";
  logstr +=
      info.cellName + "," + QString::number(batlab()->getSerialNumber()) + ",";
  logstr += QString::number(info.slot) + ",";
  logstr += QDateTime::fromTime_t(
                static_cast<uint>(std::chrono::system_clock::to_time_t(m_ts)))
                .toString("MM/dd/yyyy hh:mm:ss AP") +
            ",,,,,,,";
  logstr += type + ",";
  logstr += QString::number(static_cast<double>(m_q), 'f', 4) + ",";
  logstr += QString::number(static_cast<double>(m_e), 'f', 4) + ",";
  logstr += QString::number(static_cast<double>(m_z_avg), 'f', 4) + ",";
  logstr += QString::number(static_cast<double>(m_delta_t), 'f', 4) + ",";
  logstr += QString::number(static_cast<double>(m_current_avg), 'f', 4) + ",";
  logstr += QString::number(static_cast<double>(m_voltage_avg), 'f', 4) + ",";
  logstr +=
      QString::number(static_cast<double>(runtime.count()), 'f', 4) + ",\n";
  summaryFile.write(logstr.toUtf8());

  m_voltage_count = 0;
  m_current_count = 0;
  m_z_count = 0;

  QVector<BatlabPacket> packets;

  packets.append(BatlabPacket(info.slot, TEMPERATURE));
  // Writing to CHARGEH automatically clears CHARGEL
  packets.append(
      BatlabPacket(info.slot, CHARGEH, 0).setSleepAfterTransaction_ms(2000));

  batlabPacketBundle packetBundle;
  packetBundle.packets = packets;
  packetBundle.callback = "handleLogLvl2Response";
  packetBundle.channel = info.slot;
  batlab()->sendPacketBundle(packetBundle);
}

void Channel::logLvl3() {
  QDir resultsDir(playlist().getPlaylistOutputDirectory());
  QFile summaryFile(
      resultsDir.absoluteFilePath(playlist().getCellPlaylistName() + ".csv"));
  if (!resultsDir.exists()) {
    resultsDir.mkpath(resultsDir.path());
  }
  if (!summaryFile.exists()) {
    if (!summaryFile.open(QIODevice::WriteOnly)) {
      qWarning() << "Unable to create summary results file";
      return;
    }
    summaryFile.write(QString("\"" + playlist().toJson().replace("\"", "\"\"") +
                              "\",,,,,,,,,,,,,,,,,,,,,,,,,\n")
                          .toUtf8());
    summaryFile.write(
        "Cell Name,Batlab Serial,Channel,Timestamp (s),Voltage (V),Current "
        "(A),Temperature (C),Impedance (Ohm),Energy (J),Charge (Coulombs),Test "
        "State,Charge Capacity (Coulombs),Energy Capacity (J),Avg "
        "Impedance (Ohm),delta Temperature (C),Average Current (A),Average "
        "Voltage,Runtime (s),VCC "
        "(V),Charge Capacity,Charge Capacity Range,Energy Capacity,Energy "
        "Capacity Range,Impedance,Average Voltage,"
        "Average Current\n");
    summaryFile.close();
  }

  // Init lvl3 params
  float charge_capacity = 0;
  float charge_capacity_range = 0;
  float energy_capacity = 0;
  float energy_capacity_range = 0;
  float impedance = 0;
  float average_voltage = 0;
  float average_current = 0;

  // Calculations

  // Calculate middle 50% state of charge, and get all discharge currents and
  // voltages
  QFile cellFile(resultsDir.absoluteFilePath(playlist().getCellPlaylistName() +
                                             "_" + info.cellName + ".csv"));
  if (!cellFile.exists()) {
    qWarning() << tr("No cell results file found for %1").arg(info.cellName);
  }
  if (!cellFile.open(QIODevice::ReadOnly)) {
    qWarning() << summaryFile.errorString();
    return;
  }
  QVector<float> charges;
  QVector<float> currents;
  QVector<float> voltages;
  QString line;
  while (!line.startsWith("Cell Name") &&
         !cellFile.atEnd()) { // JSON and headers
    line = cellFile.readLine();
  }
  while (!cellFile.atEnd()) { // Data
    line = cellFile.readLine();
    auto values = QString(line).remove('"').split(',');
    if (values[9] != "") { // Charge
      charges.push_back(values[9].toFloat());
    }
    if (values[10] == "DISCHARGE" && values[4] != "") { // Voltage
      voltages.push_back(values[4].toFloat());
    }
    if (values[10] == "DISCHARGE" && values[5] != "") { // Current
      currents.push_back(values[5].toFloat());
    }
  }
  cellFile.close();
  if (charges.size() > 1) {
    float charges_min = *std::min_element(charges.begin(), charges.end());
    float charges_max = *std::max_element(charges.begin(), charges.end());
    float charges_range = charges_max - charges_min;
    float accept_impedance_with_charge_above =
        charges_min + 0.25f * charges_range;
    float accept_impedance_with_charge_below =
        charges_max - 0.25f * charges_range;

    // Get all impedance values in this range
    if (!cellFile.open(QIODevice::ReadOnly)) {
      qWarning() << summaryFile.errorString();
      return;
    }
    QVector<float> impedances;
    QString line;
    while (!line.startsWith("Cell Name") &&
           !cellFile.atEnd()) { // JSON and headers
      line = cellFile.readLine();
    }
    while (!cellFile.atEnd()) { // Data
      line = cellFile.readLine();
      auto values = QString(line).remove('"').split(',');
      if (values[9] != "" && values[7] != "") { // Impedance and charge
        if (accept_impedance_with_charge_above <= values[9].toFloat() &&
            values[9].toFloat() <= accept_impedance_with_charge_below) {
          impedances.push_back(values[7].toFloat());
        }
      }
    }
    cellFile.close();
    // Get median of accepted impedance values
    const auto median_it = impedances.begin() + impedances.size() / 2;
    std::nth_element(impedances.begin(), median_it, impedances.end());
    impedance = *median_it;
  }

  // Calculate mean current and voltage
  if (currents.size() > 0) {
    average_current = std::accumulate(currents.begin(), currents.end(), 0.0f) /
                      currents.size();
  }
  if (voltages.size() > 0) {
    average_voltage = std::accumulate(voltages.begin(), voltages.end(), 0.0f) /
                      voltages.size();
  }

  // Get all lvl2 discharge charge capacities and discharge energy capacities
  if (!summaryFile.open(QIODevice::ReadOnly)) {
    qWarning() << summaryFile.errorString();
    return;
  }
  line = "";
  QVector<float> charge_capacities;
  QVector<float> energy_capacities;
  while (!line.startsWith("Cell Name") &&
         !summaryFile.atEnd()) { // JSON and headers
    line = summaryFile.readLine();
  }
  while (!summaryFile.atEnd()) { // Data
    line = summaryFile.readLine();
    auto values = QString(line).remove('"').split(',');
    if (values[10] == "DISCHARGE" && values[11] != "") { // Charge capacity
      charge_capacities.push_back(values[11].toFloat());
    }
    if (values[10] == "DISCHARGE" && values[12] != "") { // Energy capacity
      energy_capacities.push_back(values[12].toFloat());
    }
  }

  // Take the last discharge charge capacity/energy capacity
  // Also calculate charge capacity/energy capacity range
  if (!charge_capacities.empty()) {
    float charge_capacities_min =
        *std::min_element(charge_capacities.begin(), charge_capacities.end());
    float charge_capacities_max =
        *std::max_element(charge_capacities.begin(), charge_capacities.end());
    charge_capacity = *charge_capacities.end();
    charge_capacity_range = charge_capacities_max - charge_capacities_min;
  }
  if (!energy_capacities.empty()) {
    float energy_capacities_min =
        *std::min_element(energy_capacities.begin(), energy_capacities.end());
    float energy_capacities_max =
        *std::max_element(energy_capacities.begin(), energy_capacities.end());
    energy_capacity = *energy_capacities.end();
    energy_capacity_range = energy_capacities_max - energy_capacities_min;
  }

  // Log results
  QString logstr = "";
  logstr +=
      info.cellName + "," + QString::number(batlab()->getSerialNumber()) + ",";
  logstr += QString::number(info.slot) + ",";
  logstr += QDateTime::fromTime_t(
                static_cast<uint>(std::chrono::system_clock::to_time_t(m_ts)))
                .toString("MM/dd/yyyy hh:mm:ss AP") +
            ",,,,,,,";
  logstr += "SUMMARY,,,,,,,,";
  logstr += QString::number(static_cast<double>(charge_capacity), 'f', 3) + ",";
  logstr +=
      QString::number(static_cast<double>(charge_capacity_range), 'f', 3) + ",";
  logstr += QString::number(static_cast<double>(energy_capacity), 'f', 3) + ",";
  logstr +=
      QString::number(static_cast<double>(energy_capacity_range), 'f', 3) + ",";
  logstr += QString::number(static_cast<double>(impedance), 'f', 1) + ",";
  logstr += QString::number(static_cast<double>(average_voltage), 'f', 3) + ",";
  logstr += QString::number(static_cast<double>(average_current), 'f', 3) + ",";

  summaryFile.write(logstr.toUtf8());

  info.testInProgress = false;
  auto batlabManager = dynamic_cast<BatlabManager *>(batlab()->parent());
  batlabManager->m_cellResults[info.cellName].testInProgress = false;
  batlabManager->m_cellResults[info.cellName].hasSomeResults = true;
  batlabManager->m_cellResults[info.cellName].hasCompleteResults = true;
  batlabManager->m_cellResults[info.cellName].impedance = impedance;
  batlabManager->m_cellResults[info.cellName].avgVoltage = average_voltage;
  batlabManager->m_cellResults[info.cellName].avgCurrent = average_current;
  batlabManager->m_cellResults[info.cellName].chargeCapacity = charge_capacity;
  batlabManager->m_cellResults[info.cellName].chargeCapacityRange =
      charge_capacity_range;
  batlabManager->m_cellResults[info.cellName].energyCapacity = energy_capacity;
  batlabManager->m_cellResults[info.cellName].energyCapacityRange =
      energy_capacity_range;
  emit resultsUpdated();
}

void Channel::handleLogLvl2Response(QVector<BatlabPacket> response) {
  m_temperature0 = response[0].asTemperatureC(info.tempCalibR, info.tempCalibB);
}

void Channel::completeTest() {
  abortTest();

  // Log lvl3 in file. Assume lvl2 already logged
  logLvl3();

  qInfo() << tr("Test completed on cell %1").arg(info.cellName);
}

void Channel::impedance() {
  QVector<BatlabPacket> packets;

  packets.append(BatlabPacket(info.slot, MODE).setSleepAfterTransaction_ms(10));
  packets.append(BatlabPacket(info.slot, MODE, MODE_IMPEDANCE)
                     .setSleepAfterTransaction_ms(2000));
  packets.append(BatlabPacket(UNIT, unitNamespace::LOCK, LOCK_LOCKED));
  packets.append(BatlabPacket(info.slot, CURRENT_PP));
  packets.append(BatlabPacket(info.slot, VOLTAGE_PP));
  packets.append(BatlabPacket(UNIT, unitNamespace::LOCK, LOCK_UNLOCKED));

  batlabPacketBundle packetBundle;
  packetBundle.packets = packets;
  packetBundle.callback = "handleImpedanceResponse";
  packetBundle.channel = info.slot;
  batlab()->sendPacketBundle(packetBundle);
}

void Channel::handleImpedanceResponse(QVector<BatlabPacket> response) {
  int responseCounter = 0;
  auto mode = response[responseCounter++];
  responseCounter++; // Write impedance mode
  responseCounter++; // Lock
  auto imag = response[responseCounter++].asCurrent();
  auto vmag = response[responseCounter++].asVoltage();
  responseCounter++; // Unlock

  setMode(mode.getValue());

  auto z = 0.0f;
  if (std::isnan(imag) || std::isnan(vmag)) {
    z = NAN;
  } else if (imag < 0.000001f) {
    z = 0;
  } else {
    z = vmag / imag;
  }

  if (std::isnan(z)) {
    z = m_z_avg;
    qWarning() << "Error in impedance measurement... using previous result";
  }
  m_last_impedance_time = std::chrono::system_clock::now();
  m_z_count++;
  m_z_avg = (z - m_z_avg) / m_z_count;

  QString logstr = "";
  logstr +=
      info.cellName + "," + QString::number(batlab()->getSerialNumber()) + ",";
  logstr += QString::number(info.slot) + ",";
  logstr += QDateTime::fromTime_t(
                static_cast<uint>(std::chrono::system_clock::to_time_t(m_ts)))
                .toString("MM/dd/yyyy hh:mm:ss AP") +
            ",";
  logstr += ",,," + QString("%1").arg(static_cast<double>(z), 4) + ",,";
  logstr += QString::number(static_cast<double>(m_last_charge), 'f', 4) + ",";
  logstr += L_TEST_STATE[m_test_state] + ",,,,,,,,,,,,\n";
  logLvl1(logstr);
}

// Set the mode and then the response will check that it actually got set and
// keep trying until it does
void Channel::setMode(int mode) {
  m_set_mode = mode;

  QVector<BatlabPacket> packets;

  packets.append(BatlabPacket(info.slot, MODE, static_cast<uint16_t>(mode))
                     .setSleepAfterTransaction_ms(10));
  packets.append(BatlabPacket(info.slot, MODE));

  batlabPacketBundle packetBundle;
  packetBundle.packets = packets;
  packetBundle.callback = "handleSetModeResponse";
  packetBundle.channel = info.slot;
  batlab()->sendPacketBundle(packetBundle);
}

void Channel::handleSetModeResponse(QVector<BatlabPacket> response) {
  if (response[1].asMode() != m_set_mode) {
    setMode(m_set_mode);
  }
}

void Channel::periodicCheck() {
  if (batlab()->inBootloader()) {
    return;
  }

  m_ts = std::chrono::system_clock::now();

  QVector<BatlabPacket> checkPackets;
  checkPackets.append(BatlabPacket(info.slot, cellNamespace::MODE)
                          .setSleepAfterTransaction_ms(10));
  checkPackets.append(BatlabPacket(info.slot, cellNamespace::CURRENT_SETPOINT));

  if (m_test_state != TS_IDLE) {
    checkPackets.append(BatlabPacket(info.slot, cellNamespace::VOLTAGE));
    checkPackets.append(BatlabPacket(info.slot, cellNamespace::CURRENT));
    checkPackets.append(BatlabPacket(info.slot, cellNamespace::TEMPERATURE));

    checkPackets.append(
        BatlabPacket(batlabNamespaces::UNIT, unitNamespace::SETTINGS));
    checkPackets.append(BatlabPacket(info.slot, cellNamespace::CHARGEH));
    checkPackets.append(BatlabPacket(info.slot, cellNamespace::CHARGEL));
    checkPackets.append(BatlabPacket(info.slot, cellNamespace::CHARGEH));
    checkPackets.append(BatlabPacket(info.slot, cellNamespace::CHARGEL));

    checkPackets.append(BatlabPacket(info.slot, cellNamespace::DUTY));
    checkPackets.append(
        BatlabPacket(batlabNamespaces::UNIT, unitNamespace::VCC));
  }
  batlabPacketBundle packetBundle;
  packetBundle.packets = checkPackets;
  packetBundle.callback = "handleChannelPeriodicCheckResponse";
  packetBundle.channel = info.slot;
  batlab()->sendPacketBundle(packetBundle);
}

void Channel::abortTest() {
  QVector<BatlabPacket> abortPackets;
  abortPackets.append(BatlabPacket(info.slot, cellNamespace::MODE, MODE_STOPPED)
                          .setSleepAfterTransaction_ms(10));
  batlabPacketBundle packetBundle;
  packetBundle.packets = abortPackets;
  packetBundle.callback = "handleAbortTestResponse";
  packetBundle.channel = info.slot;
  batlab()->sendPacketBundle(packetBundle);
  m_test_state = TS_IDLE;
}
