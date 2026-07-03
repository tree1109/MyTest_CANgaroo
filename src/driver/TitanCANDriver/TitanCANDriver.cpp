#include "TitanCANDriver.h"

#include "TitanCANInterface.h"
#include "core/Backend.h"
#include "driver/GenericCanSetupPage.h"

#include <QSerialPortInfo>

TitanCANDriver::TitanCANDriver(Backend &backend)
    : CanDriver(backend)
    , setupPage(new GenericCanSetupPage())
{
    QObject::connect(&backend, &Backend::onSetupDialogCreated,
                     setupPage, &GenericCanSetupPage::onSetupDialogCreated);
}

TitanCANDriver::~TitanCANDriver()
{

}

QString TitanCANDriver::getName() const
{
    return "TitanCAN";
}

bool TitanCANDriver::update()
{
    deleteAllInterfaces();

    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    for (const auto& port : std::as_const(ports)) {
        QString name = QString("TitanCAN on %1").arg(port.portName());

        createOrUpdateInterface(port.portName(), name);
    }

    return true;
}

TitanCANInterface *TitanCANDriver::createOrUpdateInterface(QString comPort, QString description)
{
    for (auto *pBusInterface : getInterfaces()) {
        TitanCANInterface * const pInterface = dynamic_cast<TitanCANInterface*>(pBusInterface);
        if (pInterface && pInterface->getComPort() == comPort) {
            pInterface->setName(description);
            return pInterface;
        }
    }

    TitanCANInterface * const pNewInterface = new TitanCANInterface(this, comPort, description);
    addInterface(pNewInterface);
    return pNewInterface;
}