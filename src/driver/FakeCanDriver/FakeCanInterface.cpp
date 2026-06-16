#include "FakeCanInterface.h"
#include "FakeCanDriver.h"

#include <chrono>

#include <QCanBus>
#include <QCanBusDevice>
#include <QCanBusFrame>
#include <QMutexLocker>

#include "core/Backend.h"
#include "core/BusMessage.h"
#include "core/MeasurementInterface.h"

FakeCanInterface::FakeCanInterface(FakeCanDriver *driver, QString deviceName, QString description)
    : BusInterface(reinterpret_cast<CanDriver*>(driver)),
    _deviceName(deviceName),
    _name(description),
    _bitrate(500000),
    _listenOnly(false)
{
    memset(&_stats, 0, sizeof(_stats));
    memset(&_offset_stats, 0, sizeof(_offset_stats));

    m_pElapsedTimer = new QElapsedTimer();
    m_pElapsedTimer->start();
}

FakeCanInterface::~FakeCanInterface()
{
    if (isOpen()) {
        close();
    }
}

QString FakeCanInterface::getName() const
{
    return _name;
}

void FakeCanInterface::setName(QString name)
{
    _name = name;
}

QList<CanTiming> FakeCanInterface::getAvailableBitrates()
{
    QList<CanTiming> retval;
    QList<unsigned> bitrates({10000, 20000, 50000, 83333, 100000, 125000,
                              250000, 500000, 800000, 1000000});
    unsigned i = 0;
    for (unsigned br : bitrates) {
        retval << CanTiming(i++, br, 0, 875);
    }
    return retval;
}

void FakeCanInterface::applyConfig(const MeasurementInterface &mi)
{
    if (!mi.doConfigure()) {
        log_info(QString("FakeCanInterface %1: not managed by cangaroo, skipping configuration")
                     .arg(_name));
        return;
    }
    _bitrate    = mi.bitrate();
    _listenOnly = mi.isListenOnlyMode();
    log_info(QString("FakeCanInterface %1: configuration stored, bitrate=%2")
                 .arg(_name).arg(_bitrate));
}

unsigned FakeCanInterface::getBitrate()
{
    return _bitrate;
}

uint32_t FakeCanInterface::getCapabilities()
{
    return BusInterface::capability_listen_only;
}

void FakeCanInterface::open()
{
    QString errorString;

    _bitrate = 114514;

    _openEpochUs = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count();
    _isOpen.store(true);
    log_info(QString("FakeCanInterface %1: opened").arg(_name));



}

bool FakeCanInterface::isOpen()
{
    return _isOpen.load();
}

void FakeCanInterface::close()
{
    _isOpen.store(false);
    log_info(QString("FakeCanInterface %1: closed").arg(_name));
}

void FakeCanInterface::sendMessage(const BusMessage &msg)
{
    if (!isOpen()) {
        log_error(QString("FakeCanInterface %1: cannot send, interface not open").arg(_name));
        return;
    }

    QCanBusFrame frame;
    frame.setFrameId(msg.getId());
    frame.setExtendedFrameFormat(msg.isExtended());

    if (msg.isRTR()) {
        frame.setFrameType(QCanBusFrame::RemoteRequestFrame);
    } else {
        frame.setFrameType(QCanBusFrame::DataFrame);
        uint8_t len = (msg.getLength() > 8) ? 8 : msg.getLength();
        QByteArray payload(len, 0);
        for (int i = 0; i < len; i++) {
            payload[i] = static_cast<char>(msg.getByte(i));
        }
        frame.setPayload(payload);
    }

    {
        _stats.tx_count++;
        addFrameBits(msg);

        BusMessage txMsg = msg;
        txMsg.setRX(false);
        auto now = std::chrono::system_clock::now().time_since_epoch();
        txMsg.setTimestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
        QMutexLocker lock(&_txMutex);
        _txMsgList.append(txMsg);
    }
}

bool FakeCanInterface::readMessage(QList<BusMessage> &msglist, unsigned int timeout_ms)
{
    if (!isOpen()) {
        return false;
    }

    // Enqueue tx messages
    {
        QMutexLocker lock(&_txMutex);
        msglist.append(_txMsgList);
        _txMsgList.clear();
    }
    bool hasTx = !msglist.isEmpty();
    if (hasTx)
    {
        timeout_ms = 1;
    }

    BusMessage msg;
    msg.setId(100);
    msg.setExtended(false);
    msg.setRTR(true);
    msg.setErrorFrame(false);
    msg.setInterfaceId(getId());

    const QCanBusFrame::TimeStamp ts = QDateTime::currentMSecsSinceEpoch();
    msg.setTimestamp_us(_openEpochUs
                        + static_cast<int64_t>(ts.seconds()) * 1000000LL
                        + static_cast<int64_t>(ts.microSeconds()));

    QByteArray fakePayload;

    const double weightF = 100 + 25 * std::cos(getElapsedSecond() / 5);
    const double heightF = 100 + 50 * std::sin(getElapsedSecond() / 10);
    const double temperatureF = std::fmod(getElapsedSecond(), 100) * 100;
    const uint16_t weight = static_cast<uint16_t>(weightF);
    const uint16_t height = static_cast<uint16_t>(heightF);
    const uint16_t temperature = static_cast<uint16_t>(temperatureF);

    fakePayload.append(weight & 0x00FF);
    fakePayload.append((weight & 0xFF00) >> 8);
    fakePayload.append(height & 0x00FF);
    fakePayload.append((height & 0xFF00) >> 8);
    fakePayload.append(temperature & 0x00FF);
    fakePayload.append((temperature & 0xFF00) >> 8);


    uint8_t len = static_cast<uint8_t>(qMin(fakePayload.size(), 8));
    msg.setLength(len);
    for (int i = 0; i < len; i++) {
        msg.setByte(i, static_cast<uint8_t>(fakePayload.at(i)));
    }

    msglist.append(msg);
    _stats.rx_count++;
    addFrameBits(msg);
    return true;
}

bool FakeCanInterface::updateStatistics()
{
    return isOpen();
}

void FakeCanInterface::resetStatistics()
{
    _offset_stats = _stats;
    BusInterface::resetStatistics();
}

uint32_t FakeCanInterface::getState()
{
    if (!isOpen()) {
        return state_stopped;
    }

    return state_ok;
}

int FakeCanInterface::getNumRxFrames()   { return static_cast<int>(_stats.rx_count    - _offset_stats.rx_count);    }
int FakeCanInterface::getNumRxErrors()   { return _stats.rx_errors   - _offset_stats.rx_errors;    }
int FakeCanInterface::getNumRxOverruns() { return static_cast<int>(_stats.rx_overruns - _offset_stats.rx_overruns); }
int FakeCanInterface::getNumTxFrames()   { return static_cast<int>(_stats.tx_count    - _offset_stats.tx_count);    }
int FakeCanInterface::getNumTxErrors()   { return _stats.tx_errors   - _offset_stats.tx_errors;    }
int FakeCanInterface::getNumTxDropped()  { return static_cast<int>(_stats.tx_dropped  - _offset_stats.tx_dropped);  }

QString FakeCanInterface::getDeviceName() const
{
    return _deviceName;
}

double FakeCanInterface::getElapsedSecond() const
{
    if (!m_pElapsedTimer) return 0;
    return m_pElapsedTimer->nsecsElapsed() * 10e-9;
}
