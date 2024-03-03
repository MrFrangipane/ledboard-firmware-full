#ifndef PLATFORMIO_CALLBACKS_H
#define PLATFORMIO_CALLBACKS_H


#include <Arduino.h>

#include <ArtnetEtherENC.h>
#include <Adafruit_NeoPXL8.h>

#include "SerialProtocol.h"
#include "ILEDBoard.h"
#include "MappingTree.h"


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

    void receiveIllumination(void *vBoard, const std::vector<byte> &data) {
        SerialProtocol::IlluminationStruct illumination;
        memcpy(&illumination, data.data(), data.size());

        auto board = static_cast<ILEDBoard*>(vBoard);
        board->setIllumination(&illumination);
    }

    //
    // Mapping Tree
    void receiveMappingTreeStructure(void *vBoard, const std::vector<byte> &data) {
        SerialProtocol::MappingTreeStructureStruct mappingTreeStructure;
        memcpy(&mappingTreeStructure, data.data(), data.size());

        auto board = static_cast<ILEDBoard*>(vBoard);
        board->setMappingTreeStructure(&mappingTreeStructure);
    }

    void receiveMappingTreeLeaf(void *vBoard, const std::vector<byte> &data) {
        SerialProtocol::MappingTreeLeafStruct mappingTreeLeaf;
        memcpy(&mappingTreeLeaf, data.data(), data.size());

        auto board = static_cast<ILEDBoard*>(vBoard);
        board->updateMappingTree(&mappingTreeLeaf);
    }

    //
    // ArtNet
    void receiveArtNet(int *fpsCounter, const MappingTree &mappingTree, Adafruit_NeoPXL8 *leds, const SerialProtocol::BoardConfigurationStruct &settings, const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {
        // Universe A
        if (metadata.universe == settings.universeA) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            fpsCounter[0]++;

            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                for (int l = 0; l < mappingTree.universeA[p].size(); l++) {
                    leds->setPixelColor(
                        mappingTree.universeA[p][l],
                        Adafruit_NeoPXL8::Color(data[p * 3], data[p * 3 + 1], data[p * 3 + 2])
                    );
                }
            }
        }

        // Universe B
        if (metadata.universe == settings.universeB) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            fpsCounter[1]++;

            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                for (int l = 0; l < mappingTree.universeB[p].size(); l++) {
                    leds->setPixelColor(
                            mappingTree.universeB[p][l],
                            Adafruit_NeoPXL8::Color(data[p * 3], data[p * 3 + 1], data[p * 3 + 2])
                    );
                }
            }
        }

        // Universe C
        if (metadata.universe == settings.universeC) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            fpsCounter[2]++;

            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                for (int l = 0; l < mappingTree.universeC[p].size(); l++) {
                    leds->setPixelColor(
                            mappingTree.universeC[p][l],
                            Adafruit_NeoPXL8::Color(data[p * 3], data[p * 3 + 1], data[p * 3 + 2])
                    );
                }
            }
        }
    }
}


#endif //PLATFORMIO_CALLBACKS_H
