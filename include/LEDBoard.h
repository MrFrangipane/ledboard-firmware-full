#ifndef PLATFORMIO_LEDBOARD_H
#define PLATFORMIO_LEDBOARD_H


#include <LittleFS.h>

#include <ArtnetEtherENC.h>
#include <Adafruit_NeoPXL8.h>

#include "Callbacks.h"
#include "ILEDBoard.h"
#include "SerialProtocol.h"
#include "SerialCommunicator.h"

#ifdef WIREOLED
#include "WireOled.h"

#else
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#endif


namespace Frangitron {

    class LEDBoard : public ILEDBoard {
    public:
        void init() {
            // SERIAL COMMUNICATION
            serialCommunicator.setCallbackParent(this);

            serialCommunicator.registerSendCallback(
                SerialProtocol::DataTypeCode::BoardConfigurationStructCode,
                sendSettings
            );
            serialCommunicator.registerReceiveCallback(
                    SerialProtocol::DataTypeCode::BoardConfigurationStructCode,
                    receiveSettings
            );

            serialCommunicator.registerSendCallback(
                    SerialProtocol::DataTypeCode::IlluminationStructCode,
                    sendIllumination
            );
            serialCommunicator.registerReceiveCallback(
                SerialProtocol::DataTypeCode::IlluminationStructCode,
                receiveIllumination
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
                receiveArtNet(fpsCounter, leds, settings, data, size, metadata, remote);
            });
            artnetReceiver.begin();

            // DISPLAY
//            display.init();
//            display.setContrast(0);
//            display.clear();

            ready = true;
        }

        void loop() {
//            artnetReceiver.parse();
//            leds->show();
        }

        void loop1() {
            if (!ready) { return; }

//            display.pollScreensaver();
            serialCommunicator.poll();

            if (millis() - fpsTimestamp > 2000) {
                for (int u=0; u < 3; u++) {
                    fps[u] = static_cast<float>(fpsCounter[u]) / 2.0;
                    fpsCounter[u] = 0;
                    fpsTimestamp = millis();
                }
            }

//            displayWrite(0, 0, Ethernet.localIP().toString() + "    ");
            displayWrite(0, 0, String(fps[0]) + " ");
            displayWrite(0, 7, String(fps[1]) + " ");
            displayWrite(1, 0, String(fps[2]) + " ");
        }

        void displayWrite(uint8_t row, uint8_t column, String text) override {
//            display.write(row, column, text);
        }

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

        const void* getIllumination() override {
            return reinterpret_cast<void*>(&illumination);
        }

        void setIllumination(const void* illumination1) override {
            memcpy(&illumination, illumination1, sizeof(SerialProtocol::IlluminationStruct));

            leds->clear();
            if(illumination.type == 0) { // Range
                for (int i = illumination.ledFirst; i <= illumination.ledLast; i++) {
                    leds->setPixelColor(i, illumination.r, illumination.g, illumination.b, illumination.w);
                }
            } else { // Single
                leds->setPixelColor(illumination.ledSingle, illumination.r, illumination.g, illumination.b, illumination.w);
            }
            leds->show();
        }

    private:
        SerialCommunicator serialCommunicator;
#ifdef  WIREOLED
        WireOled display;
#else

#endif
        SerialProtocol::BoardConfigurationStruct settings;
        SerialProtocol::IlluminationStruct illumination;
        ArtnetReceiver artnetReceiver;
        Adafruit_NeoPXL8 *leds = nullptr;
        int fpsCounter[3] = {0, 0, 0};
        unsigned long fpsTimestamp = 0;
        double fps[3] = {0.0, 0.0, 0.0};
        bool ready = false;

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
                leds = new Adafruit_NeoPXL8(settings.pixelPerTransmitter, pins, NEO_GRBW);
            } else if (settings.pixelType == 1) {
                leds = new Adafruit_NeoPXL8(settings.pixelPerTransmitter, pins, NEO_GRB);
            }

            leds->begin();
            leds->setBrightness(255);
            leds->fill(0x00050000);  // dark red
        }
    };
}

#endif //PLATFORMIO_LEDBOARD_H
