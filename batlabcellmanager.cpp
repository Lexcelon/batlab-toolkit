#include "batlabcellmanager.h"
#include <QFile>

batlabCellManager::batlabCellManager()
{

}

batlabCellManager::~batlabCellManager()
{
    uchar temp;
    foreach(temp,cells.uniqueKeys()) {
        cells.remove(temp);
    }
}


//void batlabCellManager::onReceiveStream(int unit,int cell,int status,float temp,int current,int voltage,int charge) {
//    uchar key = uchar(((unit<<2) + cell));
//    if (cells.contains(key)) {
//        cells[key]->receiveStream( status, temp, current, voltage, charge);
//    } else {
//        onNewCell(key);
//    }
//}

void batlabCellManager::onReceiveStream(int cell, int mode, int status, float temp, float current, float voltage) {
    uchar key = uchar(cell);
    if (cells.contains(key)) {
        cells[key]->receiveStream( mode, status, temp, current, voltage);
    } else {
        onNewCell(key);
    }
}

void batlabCellManager::onGetTests(uchar key) {
    if (cells.contains(key)) {
        emit emitTests(cells[key]->getTests());
    }
}

void batlabCellManager::onNewCell(uchar key) {
    cells.insert(key,new batlabCell(key));
    connect(cells[key],SIGNAL(testFinished(uchar)),this,SLOT(onTestFinished(uchar)));
}

void batlabCellManager::onNewCell(QString id , testParms parms, double ccr, double dcr, double cap, int cycles) {
    batlabCell * tempCell = new batlabCell(id,parms,cycles);
    cellList.push_back(tempCell);
}


void batlabCellManager::onTestFinished(uchar key) {
    emit testFinished(key);
}

void batlabCellManager::onNewTest(uchar key,uchar test) {
    cells[key]->newTest(test);
}

void batlabCellManager::onDeleteCell(uchar key) {
    cells.remove(key);
}

void batlabCellManager::onPrintCell(uchar key, properties val) {
    if (cells.contains(key))
    switch(val) {
    case properties::unit:
        qDebug() << QString("Unit: ") << cells[key]->getUnit();
        break;
    case properties::cell:
        qDebug() << QString("Cell: ") << cells[key]->getCell();
        break;
    case properties::status:
        qDebug() << QString("Status: ") << cells[key]->getStatus();
        break;
    case properties::statusString:
        qDebug() << QString("Status String: ") << cells[key]->getStatusString();
        break;
    case properties::temperature:
        qDebug() << QString("Temperature: ") << *cells[key]->getTemperature();
        break;
    case properties::voltage:
        qDebug() << QString("Voltage: ") << *cells[key]->getVoltage();
        break;
    case properties::current:
        qDebug() << QString("Current: ") << *cells[key]->getCurrent();
        break;
    case properties::charge:
        qDebug() << QString("Charge: ") << *cells[key]->getCharge();
        break;
    case properties::currentAmplitude:
        qDebug() << QString("Current Amplitude: ") << *cells[key]->getCurrentAmplitude();
        break;
    case properties::voltagePhase:
        qDebug() << QString("Voltage Phase: ") << *cells[key]->getVoltagePhase();
        break;
    case properties::voltageAmplitude:
        qDebug() << QString("Voltage Amplitude: ") << *cells[key]->getVoltageAmplitude();
        break;
    default:
        break;
    }
}

void batlabCellManager::onCreateTestPlan(int numBatlabs) {
    numberOfBatlabs = numBatlabs;
    testPlan = new batlabTestPlan(numBatlabs,cellList);
    connect(testPlan, SIGNAL(emitAllTestsFinished()), this, SLOT(onAllTestsFinished()));
}


void batlabCellManager::onStartTests() {
    testPlan->onStartTests();
}

void batlabCellManager::onAllTestsFinished()
{
    for (int i = 0; i < cellList.size(); ++i) {
        saveLevelOneData(cellList[i]);
    }
}

void batlabCellManager::saveLevelOneData(batlabCell* cellPointer)
{
    QString id = cellPointer->getDesignator();
    testParms tempParms = cellPointer->onGetParameters();
    QVector<testPacket> tempTests = cellPointer->getTestData();


    QFile f( "projectName.blp" );

    if (f.open(QFile::Append))
    {
        QTextStream data( &f );
        QStringList strList;

        data << id + "\n";


//        for(int d = 0; d < ui->tableWidget->columnCount(); d++) {
//            strList << " " + ui->tableWidget->horizontalHeaderItem(d)->data(Qt::DisplayRole).toString() + " ";
//        }

//        data << strList.join(",") + "\n";

//        for( int r = 0; r < ui->tableWidget->rowCount(); ++r )
//        {
//            strList.clear();
//            for( int c = 0; c < ui->tableWidget->columnCount(); ++c )
//            {
//                switch(c) {
//                case 0:
//                    strList << " "+qobject_cast<QLabel*>(ui->tableWidget->cellWidget( r, c ))->text()+" ";
//                    break;
//                case 1:
//                case 2:
//                    strList << " "+qobject_cast<QSpinBox*>(ui->tableWidget->cellWidget( r, c ))->text()+" ";
//                    break;
//                case 3:
//                case 4:
//                case 5:
//                case 6:
//                case 7:
//                case 8:
//                case 9:
//                case 10:
//                case 11:
//                case 12:
//                    strList << " "+qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget( r, c ))->text()+" ";
//                    break;
//                }
        //            }
        data << strList.join( "," )+"\n";
    }
    f.close();
}
