#pragma once

#include <QString>
#include "core/Backend.h"
#include "driver/CanDriver.h"

class TitanCANInterface;
class GenericCanSetupPage;

class TitanCANDriver : public CanDriver {
public:
    TitanCANDriver(Backend &backend);
    ~TitanCANDriver() override;

    QString getName() const override;
    bool update() override;

private:
    TitanCANInterface *createOrUpdateInterface(QString comPort, QString description);
    GenericCanSetupPage *setupPage;
};
