#ifndef BATLABTESTPLAN_H
#define BATLABTESTPLAN_H

#include <QObject>
#include "globals.h"
#include "batlabcell.h"
#include "batlabtestgroup.h"
#include <QMessageBox>

class batlabTestPlan : public QObject
{
    Q_OBJECT

public:
    batlabTestPlan();
    batlabTestPlan(int numBatlabs, QVector<batlabCell*> list);
    ~batlabTestPlan();

public slots:
    void onCreatePlan();
    void onStartTests();
    void onFinishedTests(int);
    batlabTestGroup* getNextTestGroup()
    {
        if (!testGroupList.isEmpty()) {
            return testGroupList.takeFirst();
        }
        return nullptr;
    }

private:
    int numberOfBatlabs = 0;
    QVector<batlabCell*> cellList;
    QVector<batlabTestGroup*> testGroupList;
};

#endif // BATLABTESTPLAN_H
