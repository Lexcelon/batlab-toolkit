#ifndef BATLABCOM_H
#define BATLABCOM_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QVector>
#include <QDebug>
#include <QComboBox>
#include <QInputDialog>
#include <qmath.h>
#include <QFileInfo>
#include <QQueue>
#include <QStateMachine>

#include "batlablib.h"
#include "batlabcommthread.h"
#include "channel.h"

class Batlab;

struct batlabPacketBundle {
    QQueue<batlabPacket> packets;
    QString callback;
    int channel;  // -1 if not for specific channel
};

class Batlab : public QObject
{
    Q_OBJECT
public:
    Batlab(QString item, QObject *parent = 0);
    ~Batlab();

    void setAllIdle();

signals:
    void newStreamReceived(int,int,int,float,float,float);
    void registerReadInitiated (int, int, int);
    void registerWriteInitiated(int, int, int, int);

    void batlabDisconnected(QString);
    void infoUpdated();

    void batlabPacketBundleAddedToQueue();

    void error(const QString &s);

public slots:
    void initiateRegisterRead(int, int);
    void initiateRegisterWrite(int, int, int);
    void initiateFirmwareFlash(QString firmwareFilePath);

    QString getPortName() { return info.portName; }
    int getSerialNumber() { return info.serialNumberRegister; }
    batlabStatusInfo getInfo();

    void checkSerialPortError();
    void periodicCheck();
    bool hasReceivedValidResponse();
    void verifyBatlabDevice();
    void handleVerifyBatlabDeviceResponse(QQueue<batlabPacket> response);

    void addPacketBundleToQueue(batlabPacketBundle bundle);
    void processSerialQueue();
    void handleBytesWritten(qint64 bytes);

private slots:
    void serialTransaction(int timeout, const QVector<uchar> request, int sleepAfterTransaction = 0);
    void processSerialResponse(const QVector<uchar> response);
    void processSerialError(const QString &s);
    void processSerialTimeout(const QString &s);

private:
    batlabStatusInfo info;

    // TODO move to channels?
    int tempCalibB[4];
    int tempCalibR[4];

    QStateMachine batlabStateMachine;
    QState* s_unknown;
    QState* s_bootloader;
    QState* s_booted;

    QSerialPort* m_serialPort;
    QQueue<batlabPacketBundle> m_packetBundleQueue;
    batlabPacketBundle m_currentPacketBundle;
    batlabPacket m_currentPacket;
    bool m_serialWaiting;

};

#endif // BATLABCOM_H
