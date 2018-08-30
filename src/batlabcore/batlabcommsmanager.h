#ifndef BATLABCOMMSMANAGER_H
#define BATLABCOMMSMANAGER_H

#include <QObject>
#include <QSerialPort>

#include "batlablib.h"

class BatlabCommsManager : public QObject
{
    Q_OBJECT
public:
    explicit BatlabCommsManager(QString portName, QObject *parent = nullptr);

    void addPacketBundleToQueue(batlabPacketBundle bundle);
    void processSerialQueue();
    void handleBytesWritten(qint64 bytes);

signals:
    void error(const QString &s);

public slots:

private:
    QSerialPort* m_serialPort;
    QQueue<batlabPacketBundle> m_packetBundleQueue;
    batlabPacketBundle m_currentPacketBundle;
    batlabPacket m_currentPacket;
    bool m_serialWaiting;
};

#endif // BATLABCOMMSMANAGER_H
