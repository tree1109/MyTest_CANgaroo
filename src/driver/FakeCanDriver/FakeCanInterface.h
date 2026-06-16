
#pragma once

#include "../BusInterface.h"

#include <atomic>

#include <QCanBusDevice>
#include <QElapsedTimer>
#include <QMutex>

class FakeCanDriver;


class FakeCanInterface : public BusInterface {
public:
    FakeCanInterface(FakeCanDriver *driver, QString deviceName, QString description);
    ~FakeCanInterface() override;

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

    QString getDeviceName() const;

private:
    double getElapsedSecond() const;

private:
    QString           _deviceName;
    QString           _name;
    unsigned          _bitrate;
    bool              _listenOnly;
    std::atomic<bool> _isOpen{false};


    QElapsedTimer* m_pElapsedTimer = nullptr;

    struct {
        uint64_t rx_count;
        int      rx_errors;
        uint64_t rx_overruns;
        uint64_t tx_count;
        int      tx_errors;
        uint64_t tx_dropped;
    } _stats, _offset_stats;

    int64_t _openEpochUs = 0;

    QMutex _txMutex;
    QList<BusMessage> _txMsgList;
};
