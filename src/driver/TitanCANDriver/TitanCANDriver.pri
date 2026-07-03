CONFIG += c++20
QT += network

SOURCES += \
    $$PWD/TitanCANDriver.cpp \
    $$PWD/TitanCANInterface.cpp \

HEADERS  += \
    $$PWD/TitanCANDriver.h \
    $$PWD/TitanCANInterface.h \

# Extract the downloaded hearder and lib files to TITAN_CAN_API/ next to this file.
TITAN_CAN_API_DIR = $$PWD/TITAN_CAN_API

!exists($$TITAN_CAN_API_DIR/CAN_API.h) {
    error("TitanCANDriver: TitanCAN SDK not found at $$TITAN_CAN_API_DIR.")
}

INCLUDEPATH += $$TITAN_CAN_API_DIR
LIBS        += $$TITAN_CAN_API_DIR/CAN_API.lib

message("TitanCANDriver  INCLUDEPATH: $$TITAN_CAN_API_DIR")
message("TitanCANDriver LIBS:        $$TITAN_CAN_API_DIR/PCANBasic.lib")

FORMS +=
