#ifndef TESTDATA_H
#define TESTDATA_H

#include <QObject>
#include <QFile>
#include <QString>
#include <QStringList>
#include "batlabcore/batlablib.h"

class TestData : public QObject
{
    Q_OBJECT
public:
    TestData(QObject *parent = 0);

    testPacket getTestData();
    void setTestData(testPacket packet);

    void readTestData(QString filename);

private:
    testPacket testData;
};

#endif // TESTDATA_H
