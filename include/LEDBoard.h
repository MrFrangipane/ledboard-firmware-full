#ifndef PLATFORMIO_LEDBOARD_H
#define PLATFORMIO_LEDBOARD_H


#include <LittleFS.h>

#include <Adafruit_NeoPXL8.h>
#include <ArtnetEtherENC.h>
#include "WireOled.h"

#include "Callbacks.h"
#include "ILEDBoard.h"
#include "MappingTree.h"
#include "SerialCommunicator.h"
#include "SerialProtocol.h"

/*
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
*/


namespace Frangitron {

    class LEDBoard : public ILEDBoard {
    public:
        void init() {
            // SERIAL COMMUNICATION
            serialCommunicator.setCallbackParent(this);

            // Configuration
            serialCommunicator.registerSendCallback(
                SerialProtocol::DataTypeCode::BoardConfigurationStructCode,
                sendSettings
            );
            serialCommunicator.registerReceiveCallback(
                    SerialProtocol::DataTypeCode::BoardConfigurationStructCode,
                    receiveSettings
            );
            // Illumination
            serialCommunicator.registerSendCallback(
                    SerialProtocol::DataTypeCode::IlluminationStructCode,
                    sendIllumination
            );
            serialCommunicator.registerReceiveCallback(
                SerialProtocol::DataTypeCode::IlluminationStructCode,
                receiveIllumination
            );
            // MappingTree
            serialCommunicator.registerReceiveCallback(
                    SerialProtocol::DataTypeCode::MappingTreeStructureStructCode,
                    receiveMappingTreeStructure
            );
            serialCommunicator.registerReceiveCallback(
                    SerialProtocol::DataTypeCode::MappingTreeLeafStructCode,
                    receiveMappingTreeLeaf
            );

            // FILESYSTEM
            LittleFS.begin();

            // SETTINGS
            loadSettings();

            // LEDS
            initLeds();

            // NETWORK
            const IPAddress ip(
                settings.ipAddress[0],
                settings.ipAddress[1],
                settings.ipAddress[2],
                settings.ipAddress[3]
            );
            pico_unique_board_id_t boardId;
            pico_get_unique_board_id(&boardId);
            uint8_t mac[] = {
                    boardId.id[2],
                    boardId.id[3],
                    boardId.id[4],
                    boardId.id[5],
                    boardId.id[6],
                    boardId.id[7]
            };

            Ethernet.begin(mac, ip);

            // ArtNet
            artnetReceiver.subscribeArtDmx([&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {
                receiveArtNet(fpsCounter, mappingTree, leds, settings, data, size, metadata, remote);
            });
            artnetReceiver.begin();

            // DISPLAY
            display.init();
            display.setContrast(0);
            display.clear();

            // MAPPING TREE
            loadMappingTree();

            ready = true;
        }

        void loop() {
            if (settings.executionMode == 0) { // Illumination  FIXME: use enums
            }
            else { // ArtNet  FIXME: use enums
                artnetReceiver.parse();
                leds->show();
                delay(5);
            }
        }

        void loop1() {
            if (!ready) { return; }

            display.pollScreensaver();
            serialCommunicator.poll();

            if (millis() - fpsTimestamp > 2000) {
                for (int u=0; u < 3; u++) {
                    fps[u] = static_cast<float>(fpsCounter[u]) / 2.0;
                    fpsCounter[u] = 0;
                    fpsTimestamp = millis();
                }
            }

            displayWrite(0, 0, Ethernet.localIP().toString() + "  ");
            displayWrite(1, 0, String(fps[0], 1) + " ");
            displayWrite(1, 6, String(fps[1], 1) + " ");
            displayWrite(1, 12, String(fps[2], 1) + " ");

            // Illumination / Arnet
            if (settings.executionMode == 0) {
                displayWrite(0, 15, "I");
            } else {
                displayWrite(0, 15, "A");
            }
        }

        void displayWrite(uint8_t row, uint8_t column, String text) override {
            display.write(row, column, text);
        }

        //
        // Settings
        const void* getSettings() override {
            return reinterpret_cast<void*>(&settings);
        }

        void setSettings(const void* settings1) override {
            memcpy(&settings, settings1, sizeof(settings));

            bool doSaveAndReboot = settings.doSaveAndReboot;

            if (settings.eraseMappingTreeFile) {
                LittleFS.remove("ledtree.bin");
            }

            if (settings.doRebootBootloader) {
                rp2040.rebootToBootloader();
            }

            setFixedSettingsValues();

            if (doSaveAndReboot == 1) {
                saveSettings();
                rp2040.reboot();
            }
        }

        void loadSettings() override {
            if (!LittleFS.exists("settings.bin")) {
                saveSettings();

            } else {
                File f = LittleFS.open("settings.bin", "r");
                if (f.size() == sizeof(settings)) {
                    f.readBytes(reinterpret_cast<char*>(&settings), sizeof(settings));
                }
                f.close();
                setFixedSettingsValues();
            }
        }

        void saveSettings() override {
            setFixedSettingsValues();
            File f = LittleFS.open("settings.bin", "w");
            f.write(reinterpret_cast<char*>(&settings), sizeof(settings));
            f.close();

            // FIXME: hacky
            saveMappingTree();
        }

        //
        // Mapping Tree
        void setMappingTreeStructure(const void* mappingTreeStructure1) override {
            memcpy(&mappingTreeStructure, mappingTreeStructure1, sizeof(mappingTreeStructure));
            resetMappingTree();
        }

        void updateMappingTree(const void* mappingTreeLeaf1) override {
            SerialProtocol::MappingTreeLeafStruct leaf;
            memcpy(&leaf, mappingTreeLeaf1, sizeof(leaf));

            if (leaf.universeNumber == settings.universeA) {
                mappingTree.universeA[leaf.pixelNumber][leaf.mappingId] = leaf.ledId;
            }

            if (leaf.universeNumber == settings.universeB) {
                mappingTree.universeB[leaf.pixelNumber][leaf.mappingId] = leaf.ledId;
            }

            if (leaf.universeNumber ==  settings.universeC) {
                mappingTree.universeC[leaf.pixelNumber][leaf.mappingId] = leaf.ledId;
            }
        }

        void resetMappingTree() override {
            int ledId = 0;

            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                mappingTree.universeA[p].clear();
                mappingTree.universeA[p].resize(mappingTreeStructure.universeAPixelsLedCount[p]);
                for (int l = 0; l < mappingTreeStructure.universeAPixelsLedCount[p]; l++) {
                    mappingTree.universeA[p][l] = ledId;
                    ledId++;
                }
            }

            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                mappingTree.universeB[p].clear();
                mappingTree.universeB[p].resize(mappingTreeStructure.universeBPixelsLedCount[p]);
                for (int l = 0; l < mappingTreeStructure.universeBPixelsLedCount[p]; l++) {
                    mappingTree.universeB[p][l] = ledId;
                    ledId++;
                }
            }

            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                mappingTree.universeC[p].clear();
                mappingTree.universeC[p].resize(mappingTreeStructure.universeCPixelsLedCount[p]);
                for (int l = 0; l < mappingTreeStructure.universeCPixelsLedCount[p]; l++) {
                    mappingTree.universeC[p][l] = ledId;
                    ledId++;
                }
            }
        }

        void loadMappingTree() override {
            if (!LittleFS.exists("ledtree.bin")) {
                return;
            }

            File f = LittleFS.open("ledtree.bin", "r");
            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                mappingTreeStructure.universeAPixelsLedCount[p] = f.parseInt();
                mappingTreeStructure.universeBPixelsLedCount[p] = f.parseInt();
                mappingTreeStructure.universeCPixelsLedCount[p] = f.parseInt();
            }

            resetMappingTree();

            // Universe A
            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                for (int l = 0; l < mappingTreeStructure.universeAPixelsLedCount[p]; l++) {
                    mappingTree.universeA[p][l] = f.parseInt();
                }
            }

            // Universe B
            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                for (int l = 0; l < mappingTreeStructure.universeBPixelsLedCount[p]; l++) {
                    mappingTree.universeB[p][l] = f.parseInt();
                }
            }

            // Universe C
            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                for (int l = 0; l < mappingTreeStructure.universeCPixelsLedCount[p]; l++) {
                    mappingTree.universeC[p][l] = f.parseInt();
                }
            }
            f.close();
        }

        void saveMappingTree() override {
            File f = LittleFS.open("ledtree.bin", "w");

            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                f.print(mappingTreeStructure.universeAPixelsLedCount[p]); f.print(" ");
                f.print(mappingTreeStructure.universeBPixelsLedCount[p]); f.print(" ");
                f.print(mappingTreeStructure.universeCPixelsLedCount[p]); f.print(" ");
            }

            // Universe A
            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                for (int l = 0; l < mappingTreeStructure.universeAPixelsLedCount[p]; l++) {
                    f.print(mappingTree.universeA[p][l]); f.print(" ");
                }
            }

            // Universe B
            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                for (int l = 0; l < mappingTreeStructure.universeBPixelsLedCount[p]; l++) {
                    f.print(mappingTree.universeB[p][l]); f.print(" ");
                }
            }

            // Universe C
            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                for (int l = 0; l < mappingTreeStructure.universeCPixelsLedCount[p]; l++) {
                    f.print(mappingTree.universeC[p][l]); f.print(" ");
                }
            }

            f.close();
        }

        //
        // Illumination
        const void* getIllumination() override {
            return reinterpret_cast<void*>(&illumination);
        }

        void setIllumination(const void* illumination1) override {
            if (settings.executionMode == 0) { // Illumination  FIXME: use enums
                memcpy(&illumination, illumination1, sizeof(SerialProtocol::IlluminationStruct));

                leds->clear();
                if (illumination.type == 0) { // Range  FIXME: use enums
                    for (int i = illumination.ledFirst; i <= illumination.ledLast; i++) {
                        leds->setPixelColor(i, illumination.r, illumination.g, illumination.b, illumination.w);
                    }
                } else { // Single  FIXME: use enums
                    leds->setPixelColor(
                        illumination.ledSingle, illumination.r, illumination.g, illumination.b, illumination.w
                    );
                }
                leds->show();
            }
        }

    private:
        SerialCommunicator serialCommunicator;
        WireOled display;

        SerialProtocol::BoardConfigurationStruct settings;
        SerialProtocol::IlluminationStruct illumination;
        SerialProtocol::MappingTreeStructureStruct mappingTreeStructure;

        ArtnetReceiver artnetReceiver;
        Adafruit_NeoPXL8 *leds = nullptr;
        int fpsCounter[3] = {0, 0, 0};
        unsigned long fpsTimestamp = 0;
        double fps[3] = {0.0, 0.0, 0.0};
        bool ready = false;
        MappingTree mappingTree;

        void setFixedSettingsValues() {
            settings.hardwareRevision = 1;

            settings.firmwareRevision = 0;

            pico_unique_board_id_t boardId;
            pico_get_unique_board_id(&boardId);
            settings.hardwareId[0] = boardId.id[0];
            settings.hardwareId[1] = boardId.id[1];
            settings.hardwareId[2] = boardId.id[2];
            settings.hardwareId[3] = boardId.id[3];
            settings.hardwareId[4] = boardId.id[4];
            settings.hardwareId[5] = boardId.id[5];
            settings.hardwareId[6] = boardId.id[6];
            settings.hardwareId[7] = boardId.id[7];

            settings.eraseMappingTreeFile = false;
            settings.doSaveAndReboot = false;
            settings.doRebootBootloader = false;

            settings.pixelPerUniverse = 128;
        }

        void initLeds() {
            int8_t pins[8] = {6, 7, 8, 9, 10, 11, 12, 13};

            delete leds;
            if (settings.pixelType == 0) {
                leds = new Adafruit_NeoPXL8(settings.ledPerTransmitter, pins, NEO_GRBW);
            } else if (settings.pixelType == 1) {
                leds = new Adafruit_NeoPXL8(settings.ledPerTransmitter, pins, NEO_GRB);
            }

            leds->begin();
            leds->setBrightness(255);
            leds->fill(0x00050000);  // dark red
        }
    };
}

#endif //PLATFORMIO_LEDBOARD_H
