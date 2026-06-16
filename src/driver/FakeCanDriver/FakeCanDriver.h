
#pragma once

#include <QString>
#include "core/Backend.h"
#include "driver/CanDriver.h"

class FakeCanInterface;
class GenericCanSetupPage;

class FakeCanDriver : public CanDriver {
public:
    FakeCanDriver(Backend &backend);
    ~FakeCanDriver() override;

    QString getName() const override;
    bool update() override;

private:
    FakeCanInterface *createOrUpdateInterface(QString deviceName, QString description);
    GenericCanSetupPage *setupPage;
};
