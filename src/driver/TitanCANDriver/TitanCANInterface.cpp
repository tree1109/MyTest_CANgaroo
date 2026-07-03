#include "TitanCANInterface.h"
#include "TitanCANDriver.h"

#include <chrono>

#include <QCanBus>
#include <QCanBusDevice>
#include <QCanBusFrame>
#include <QMutexLocker>

#include "core/Backend.h"
#include "core/BusMessage.h"
#include "core/MeasurementInterface.h"

#include "TITAN_CAN_API/CAN_API.h"

namespace {
    QString GetTitanCANStatusErrorMessage(const TCAN_STATUS status) {
        switch (status) {
        case CAN_ERR_OK: return "Ok";
        case CAN_ERR_ERR: return "Error";
        case CAN_ERR_OPEN_CHANNEL: return "Error open channel";
        case CAN_ERR_PARAMETER: return "Error parameter";
        case CAN_ERR_NOT_OPEN: return "Error not open";
        case CAN_ERR_READ_NO_MSG: return "Error read not message";
        default: return "Unknow status";
        }
    }

    uint32_t GetTitanCANBitrate(uint32_t bitrate) {
        switch(bitrate)
        {
            case 10000: return 10;
            case 20000: return 20;
            case 50000: return 50;
            case 100000: return 100;
            case 125000: return 125;
            case 250000: return 250;
            case 500000: return 500;
            case 800000: return 800;
            case 1000000: return 1000;
            default: return 500;
        }
    }
}

TitanCANInterface::TitanCANInterface(TitanCANDriver *driver, QString comPort, QString name)
    : BusInterface(reinterpret_cast<CanDriver*>(driver)),
    _name(name),
    _comPort(comPort),
    _bitrate(500000),
    _listenOnly(false)
{
    memset(&_stats, 0, sizeof(_stats));
    memset(&_offset_stats, 0, sizeof(_offset_stats));

}

TitanCANInterface::~TitanCANInterface()
{
    if (TitanCANInterface::isOpen()) {
        TitanCANInterface::close();
    }
}

QString TitanCANInterface::getName() const
{
    return _name;
}

void TitanCANInterface::setName(QString name)
{
    _name = name;
}

QList<CanTiming> TitanCANInterface::getAvailableBitrates()
{
    QList<CanTiming> retval;
    QList<uint32_t> bitrates({10000, 20000, 50000, 100000, 125000,
                              250000, 500000, 800000, 1000000});
    uint32_t i = 0;
    for (uint32_t br : bitrates) {
        retval << CanTiming(i++, br, 0, 875);
    }
    return retval;
}

void TitanCANInterface::applyConfig(const MeasurementInterface &mi)
{
    if (!mi.doConfigure()) {
        log_info(QString("TitanCANInterface %1: not managed by cangaroo, skipping configuration")
                     .arg(_name));
        return;
    }
    _bitrate    = mi.bitrate();
    _listenOnly = mi.isListenOnlyMode();
    log_info(QString("TitanCANInterface %1: configuration stored, bitrate=%2")
                 .arg(_name).arg(_bitrate));
}

unsigned TitanCANInterface::getBitrate()
{
    return _bitrate;
}

uint32_t TitanCANInterface::getCapabilities()
{
    return BusInterface::capability_listen_only;
}

void TitanCANInterface::open()
{
    QString errorString;

    // Get settings.
    const int32_t bitrate = GetTitanCANBitrate(_bitrate);
    const uint32_t acceptanceCode = 0x1FFFFFFF;
    const uint32_t acceptanceMask = 0x00000000;

    // Open CAN.
    {
        // NOTE: We need convert to std string to use char*.
        const auto comPortStr = _comPort.toStdString();
        const auto bitrateStr = QString::number(bitrate).toStdString();
        const auto acceptanceCodeStr = QString("%1").arg(acceptanceCode, 8, 16, QChar('0')).toUpper().toStdString();
        const auto acceptanceMaskStr = QString("%1").arg(acceptanceMask, 8, 16, QChar('0')).toUpper().toStdString();
        const void* const flags = CAN_TIMESTAMP_ON;
        const auto mode = _listenOnly ? ListenOnly : Normal;
        // const auto mode = LoopBack; // Test only.

        _canHandle = CAN_Open(
            const_cast<char*>(comPortStr.c_str()),
            const_cast<char*>(bitrateStr.c_str()),
            const_cast<char*>(acceptanceCodeStr.c_str()),
            const_cast<char*>(acceptanceMaskStr.c_str()),
            const_cast<void*>(flags),
            mode);
    }

    // Check status.
    if (_canHandle < 0) {
        const auto errorMessage = GetTitanCANStatusErrorMessage(_canHandle);
        log_error(QString("TitanCANInterface %1: CAN_Open failed: %2").arg(_name, errorMessage));
        return;
    }

    // Flush.
    {
        const TCAN_STATUS status = CAN_Flush(_canHandle);

        if (status < 0) {
            const auto errorMessage = GetTitanCANStatusErrorMessage(status);
            log_error(QString("TitanCANInterface %1: Flush failed: %2").arg(_name, errorMessage));
        }
    }

    // Check version.
    {
        char version[BUFSIZ]{};
        const TCAN_STATUS status = CAN_Version(_canHandle, version);

        if (status == CAN_ERR_OK) {
            log_info(QString("TitanCANInterface %1: version: %2").arg(_name, version));
        }
        else {
            const auto errorMessage = GetTitanCANStatusErrorMessage(status);
            log_error(QString("TitanCANInterface %1: Get version failed: %2").arg(_name, errorMessage));
        }
    }

    // Record can open time.
    {
        using namespace std::chrono;
        const auto now = system_clock::now().time_since_epoch();
        _canOpenOpenTime_us = duration_cast<microseconds>(now).count();
    }

    log_info(QString("TitanCANInterface %1: opened").arg(_name));
}

bool TitanCANInterface::isOpen()
{
    return _canHandle > 0;
}

void TitanCANInterface::close()
{
    const TCAN_STATUS status = CAN_Close(_canHandle);

    // Check status.
    if (status < 0) {
        const auto errorMessage = GetTitanCANStatusErrorMessage(status);
        log_error(QString("TitanCANInterface %1: CAN_Close failed: %2").arg(_name, errorMessage));
        return;
    }

    _canHandle = 0;

    log_info(QString("TitanCANInterface %1: closed").arg(_name));
}

void TitanCANInterface::sendMessage(const BusMessage &msg)
{
    if (!isOpen()) {
        log_error(QString("TitanCANInterface %1: cannot send, interface not open").arg(_name));
        return;
    }

    CAN_MSG frame;
    memset(&frame, 0, sizeof(frame));

    // Convert format.
    {
        // ID.
        frame.Id = msg.getId();

        // Size.
        constexpr uint8_t MAX_DATA_LENGTH = 8;
        const uint8_t dataLength = qMin(msg.getLength(), MAX_DATA_LENGTH);
        frame.Size = dataLength;

        // Data.
        if (msg.isRTR()) {
            for (uint8_t i = 0; i < dataLength; ++i) {
                frame.Data[i] = msg.getByte(i);
            }
        }

        // Flags.
        frame.Flags |= msg.isExtended() ? CAN_FLAGS_EXTENDED : CAN_FLAGS_STANDARD;
        if (msg.isRTR()) {
            frame.Flags |= CAN_FLAGS_REMOTE;
        }

        // Timestamp.
        // NOTE: No need to set.
        frame.Timestamp = 0;
    }

    // Write message.
    const TCAN_STATUS status = CAN_Write(_canHandle, &frame);

    // Check status.
    if (status == CAN_ERR_OK) {
        // Record tx msg status.
        ++_stats.tx_count;
        addFrameBits(msg);

        // Record tx msg.
        BusMessage txMsg = msg;
        txMsg.setRX(false);
        auto now = std::chrono::system_clock::now().time_since_epoch();
        txMsg.setTimestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
        QMutexLocker lock(&_txMutex);
        _txMsgList.append(txMsg);

        log_error(QString("TitanCANInterface %1: CAN_Write send 1 message").arg(_name));
    }
    else {
        const auto errorMessage = GetTitanCANStatusErrorMessage(status);
        log_error(QString("TitanCANInterface %1: CAN_Write failed: %2").arg(_name, errorMessage));

        // Record tx msg status.
        ++_stats.tx_errors;
    }
}

bool TitanCANInterface::readMessage(QList<BusMessage> &msglist, unsigned int timeout_ms)
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

    CAN_MSG frame;
    memset(&frame, 0, sizeof(frame));

    // Read all message.
    for (;;) {
        // Read message.
        const TCAN_STATUS status = CAN_Read(_canHandle, &frame);

        if (status == CAN_ERR_OK) {
            // Convert format.
            BusMessage msg;
            {
                msg.setInterfaceId(getId());
                msg.setErrorFrame(false);

                // ID.
                msg.setId(frame.Id);

                // Size.
                constexpr uint8_t MAX_DATA_LENGTH = 8;
                const uint8_t dataLength = qMin(frame.Size, MAX_DATA_LENGTH);
                msg.setLength(dataLength);

                // Data.
                for (uint8_t i = 0; i < dataLength; ++i) {
                    msg.setByte(i, frame.Data[i]);
                }

                // Flags.
                msg.setExtended((frame.Flags & CAN_FLAGS_EXTENDED) != 0);
                msg.setRTR((frame.Flags & CAN_FLAGS_REMOTE) != 0);


                // Timestamp.
                {
                    constexpr uint64_t MS_TO_US = 1000;
                    const uint64_t device_us = frame.Timestamp * MS_TO_US;
                    const uint64_t total_us = device_us + _canOpenOpenTime_us;
                    msg.setTimestamp_us(total_us);
                }
            }

            // Record rx msg status.
            msglist.append(msg);
            _stats.rx_count++;
            addFrameBits(msg);
        }
        else if (status == CAN_ERR_READ_NO_MSG) {
            break;
        }
        else {
            const auto errorMessage = GetTitanCANStatusErrorMessage(status);
            log_error(QString("TitanCANInterface %1: CAN_Read failed: %2").arg(_name, errorMessage));

            // Record rx msg status.
            ++_stats.rx_errors;

            return false;
        }
    }

    return true;
}

bool TitanCANInterface::updateStatistics()
{
    return isOpen();
}

void TitanCANInterface::resetStatistics()
{
    _offset_stats = _stats;
    BusInterface::resetStatistics();
}

uint32_t TitanCANInterface::getState()
{
    if (!isOpen()) {
        return state_stopped;
    }

    // Get status.
    const TCAN_STATUS status = CAN_Status(_canHandle);

    if (status & CAN_STATUS_BOFF) return state_bus_off;
    if (status & CAN_STATUS_EPASS) return state_passive;
    if (status & CAN_STATUS_EWARN) return state_warning;

    return state_ok;
}

int TitanCANInterface::getNumRxFrames()   { return static_cast<int>(_stats.rx_count    - _offset_stats.rx_count);    }
int TitanCANInterface::getNumRxErrors()   { return _stats.rx_errors   - _offset_stats.rx_errors;    }
int TitanCANInterface::getNumRxOverruns() { return static_cast<int>(_stats.rx_overruns - _offset_stats.rx_overruns); }
int TitanCANInterface::getNumTxFrames()   { return static_cast<int>(_stats.tx_count    - _offset_stats.tx_count);    }
int TitanCANInterface::getNumTxErrors()   { return _stats.tx_errors   - _offset_stats.tx_errors;    }
int TitanCANInterface::getNumTxDropped()  { return static_cast<int>(_stats.tx_dropped  - _offset_stats.tx_dropped);  }

QString TitanCANInterface::getComPort() const
{
    return _comPort;
}