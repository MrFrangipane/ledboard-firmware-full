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


// FIXME: ugly
#define SEP_UNV (-1)
#define SEP_PIX (-2)


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

            ready = true;
        }

        void loop() {
            if (settings.executionMode == 0) { // Illumination  FIXME: use enums
            }
            else { // ArtNet  FIXME: use enums
                artnetReceiver.parse();
                leds->show();
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

            displayWrite(0, 0, Ethernet.localIP().toString() + "    ");
            displayWrite(1, 0, String(fps[0]) + " ");
            displayWrite(1, 6, String(fps[1]) + " ");
            displayWrite(1, 11, String(fps[2]) + " ");
        }

        void displayWrite(uint8_t row, uint8_t column, String text) override {
            display.write(row, column, text);
        }

        //
        // Settings
        const void* getSettings() override {
            displayWrite(1, 7, "> send    ");
            return reinterpret_cast<void*>(&settings);
        }

        void setSettings(const void* settings1) override {
            memcpy(&settings, settings1, sizeof(settings));

            if (settings.doRebootBootloader) {
                rp2040.rebootToBootloader();
            }

            setFixedSettingsValues();

            if (settings.doSaveAndReboot == 1) {
                displayWrite(1, 7, "< save   ");
                saveSettings();
                rp2040.reboot();
            } else {
                displayWrite(1, 7, "< no save");
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

            if (leaf.universeNumber == 0) {
                mappingTree.universeA[leaf.pixelNumber][leaf.mappingId] = leaf.ledId;
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
                mappingTree.universeB[p].reserve(mappingTreeStructure.universeBPixelsLedCount[p]);
            }

            for (int p = 0; p < settings.pixelPerUniverse; p++) {
                mappingTree.universeC[p].clear();
                mappingTree.universeC[p].reserve(mappingTreeStructure.universeCPixelsLedCount[p]);
            }
        }

        void loadMappingTree() override {
            /*
            clearMappingTree();
            if (!LittleFS.exists("ledtree.bin")) {
                return;
            }

            int universe_id = 0;
            int pixel_id = 0;

            File f = LittleFS.open("ledtree.bin", "r");
            while (f.available()) {
                int readValue = f.parseInt();
                if (readValue == SEP_UNV) {
                    pixel_id = 0;
                    universe_id++;
                    std::vector<int> pixel0;
                    ledTree[universe_id].push_back(pixel0);
                }
                else if (readValue == SEP_PIX) {
                    pixel_id++;
                    std::vector<int> pixel;
                    ledTree[universe_id].push_back(pixel);
                }
                else {
                    ledTree[universe_id][pixel_id].push_back(readValue);
                }
            }
            f.close();
             */
        }

        void saveMappingTree() override {
            /*
            File f = LittleFS.open("ledtree.bin", "w");

            for (int universe_id = 0; universe_id < ledTree.size(); universe_id ++) {
                for (int pixel_id = 0; pixel_id < ledTree[universe_id].size(); pixel_id++) {
                    for (int led_id = 0; led_id < ledTree[universe_id][pixel_id].size(); led_id++) {
                        f.print(ledTree[universe_id][pixel_id][led_id]);
                        f.print(" ");
                    }
                    f.print(SEP_PIX);
                    f.print(" ");
                }
                f.print(SEP_UNV);
                f.print(" ");
            }

            f.close();
            */
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
