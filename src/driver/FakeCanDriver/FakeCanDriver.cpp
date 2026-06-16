

#include "FakeCanDriver.h"


#include "FakeCanInterface.h"
#include "core/Backend.h"
#include "driver/GenericCanSetupPage.h"

#include <QCanBus>

FakeCanDriver::FakeCanDriver(Backend &backend)
    : CanDriver(backend)
    , setupPage(new GenericCanSetupPage())
{

    QObject::connect(&backend, &Backend::onSetupDialogCreated,
                     setupPage, &GenericCanSetupPage::onSetupDialogCreated);
}



FakeCanDriver::~FakeCanDriver()
{

}



QString FakeCanDriver::getName() const
{
    return "FakeCAN";
}



bool FakeCanDriver::update()
{
    deleteAllInterfaces();

    for (int32_t i = 0; i < 2; ++i) {
        QString name = std::format("FakeCAN {}", i).c_str();
        QString desc = std::format("FakeCAN {} for test.", i).c_str();
        createOrUpdateInterface(name, desc);
    }

    return true;
}



FakeCanInterface *FakeCanDriver::createOrUpdateInterface(QString deviceName, QString description)
{
    for (auto *intf : getInterfaces()) {
        FakeCanInterface *tif = dynamic_cast<FakeCanInterface*>(intf);
        if (tif && tif->getDeviceName() == deviceName) {
            tif->setName(description);
            return tif;
        }
    }

    FakeCanInterface *tif = new FakeCanInterface(this, deviceName, description);
    addInterface(tif);
    return tif;
}