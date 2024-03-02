#ifndef PLATFORMIO_CALLBACKS_H
#define PLATFORMIO_CALLBACKS_H


#include <Arduino.h>

#include <ArtnetEtherENC.h>
#include <Adafruit_NeoPXL8.h>

#include "SerialProtocol.h"
#include "ILEDBoard.h"


namespace Frangitron {

    //
    // Settings
    void sendSettings(void *vBoard, std::vector<byte> &data) {
        auto board = static_cast<ILEDBoard*>(vBoard);

        data.resize(sizeof(SerialProtocol::BoardConfigurationStruct)); // FIXME be more efficient
        memcpy(data.data(), board->getSettings(), sizeof(SerialProtocol::BoardConfigurationStruct));
    }

    void receiveSettings(void *vBoard, const std::vector<byte> &data) {
        SerialProtocol::BoardConfigurationStruct settings;
        memcpy(&settings, data.data(), data.size());

        auto board = static_cast<ILEDBoard*>(vBoard);
        board->setSettings(&settings);
    }

    //
    // Illumination
    void sendIllumination(void *vBoard, std::vector<byte> &data) {
        auto board = static_cast<ILEDBoard*>(vBoard);

        data.resize(sizeof(SerialProtocol::IlluminationStruct)); // FIXME be more efficient
        memcpy(data.data(), board->getIllumination(), sizeof(SerialProtocol::IlluminationStruct));
    }

    //
    // ArtNet
    void receiveIllumination(void *vBoard, const std::vector<byte> &data) {
        SerialProtocol::IlluminationStruct illumination;
        memcpy(&illumination, data.data(), data.size());

        auto board = static_cast<ILEDBoard*>(vBoard);
        board->setIllumination(&illumination);
    }


    void receiveArtNet(int *fpsCounter, Adafruit_NeoPXL8 *leds, const SerialProtocol::BoardConfigurationStruct &settings, const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {
        // TODO
    }
}


#endif //PLATFORMIO_CALLBACKS_H
