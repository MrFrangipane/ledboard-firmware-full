//
// This file was automatically generated by python-arduino-serial
// https://github.com/MrFrangipane/python-arduino-serial
//
#ifndef PLATFORMIO_SERIALPROTOCOL_H
#define PLATFORMIO_SERIALPROTOCOL_H

#include <map>

#include <Arduino.h>


namespace Frangitron {

    class SerialProtocol {
        /*
         * Message topology
         *
         * ```
         * |   0   |    1      |     2     |   n  | 3 + n |
         * | begin | direction | data type | data |  end  |
         * |-------| ------ header ------- | data |-------|
         * ```
         */
    public:
        enum class Direction : int {
            Send,
            Receive
        };

        static constexpr uint8_t headerSize = 2;
        static constexpr byte flagBegin = 0x3c;
        static constexpr byte flagEnd = 0x3e;

        /* //////////////////////////////// */

        enum DataTypeCode : int {
            BoardConfigurationStructCode,
            IlluminationStructCode
        };

        struct BoardConfigurationStruct {
            char name[8] = "       ";
            int hardwareRevision = 1;
            int firmwareRevision = 1;
            byte hardwareId[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            byte ipAddress[4] = {0x00, 0x00, 0x00, 0x00};
            int universe = -1;
            int pixelPerTransmitter = 150;
            int pixelType = 0;
            int doSaveAndReboot = 0;
            int doRebootBootloader = 0;
        };
        
        struct IlluminationStruct {
            int ledStart = 0;
            int ledEnd = 0;
            int r = 0;
            int g = 0;
            int b = 0;
            int w = 0;
        };

        static const std::map<DataTypeCode, uint16_t> DataSize;
    };

    const std::map<SerialProtocol::DataTypeCode, uint16_t> SerialProtocol::DataSize = {
        {SerialProtocol::DataTypeCode::BoardConfigurationStructCode, sizeof(SerialProtocol::BoardConfigurationStruct)},
        {SerialProtocol::DataTypeCode::IlluminationStructCode, sizeof(SerialProtocol::IlluminationStruct)}
    };
}

#endif // PLATFORMIO_SERIALPROTOCOL_H