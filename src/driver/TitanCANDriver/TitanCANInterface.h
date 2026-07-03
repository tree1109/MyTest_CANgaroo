
#pragma once

#include "../BusInterface.h"

#include <atomic>

#include <QCanBusDevice>
#include <QElapsedTimer>
#include <QMutex>

// TCAN_HANDLE is typedef'd as int in CAN_API.h.
typedef int TCAN_HANDLE;

class TitanCANDriver;

class TitanCANInterface : public BusInterface {
public:
    TitanCANInterface(TitanCANDriver *driver, QString comPort, QString name);
    ~TitanCANInterface() override;

    QString getName() const override;
    void setName(QString name);

    QList<CanTiming> getAvailableBitrates() override;

    void applyConfig(const MeasurementInterface &mi) override;

    unsigned getBitrate() override;
    uint32_t getCapabilities() override;

    void open() override;
    bool isOpen() override;
    void close() override;

    void sendMessage(const BusMessage &msg) override;
    bool readMessage(QList<BusMessage> &msglist, unsigned int timeout_ms) override;

    bool updateStatistics() override;
    void resetStatistics() override;
    uint32_t getState() override;
    int getNumRxFrames() override;
    int getNumRxErrors() override;
    int getNumRxOverruns() override;
    int getNumTxFrames() override;
    int getNumTxErrors() override;
    int getNumTxDropped() override;

    QString getComPort() const;

private:
    QString           _name;
    QString           _comPort;
    TCAN_HANDLE       _canHandle;
    uint32_t          _bitrate;
    bool              _listenOnly;
    uint64_t    _canOpenOpenTime_us;

    struct {
        uint64_t rx_count;
        int      rx_errors;
        uint64_t rx_overruns;
        uint64_t tx_count;
        int      tx_errors;
        uint64_t tx_dropped;
    } _stats, _offset_stats;

    QMutex _txMutex;
    QList<BusMessage> _txMsgList;
};
