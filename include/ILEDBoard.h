#ifndef PLATFORMIO_ILEDBOARD_H
#define PLATFORMIO_ILEDBOARD_H


#include "Arduino.h"


namespace Frangitron {

    class ILEDBoard {
    public:
        virtual ~ILEDBoard() {}

        virtual void displayWrite(uint8_t row, uint8_t column, String text) = 0;

        virtual const void* getSettings() = 0;
        virtual void setSettings(const void* settings1) = 0;
        virtual void illuminate(const void* illumination1) = 0;

        virtual void loadSettings() = 0;
        virtual void saveSettings() = 0;
    };

}

#endif //PLATFORMIO_ILEDBOARD_H
