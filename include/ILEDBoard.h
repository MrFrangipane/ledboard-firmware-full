#ifndef PLATFORMIO_ILEDBOARD_H
#define PLATFORMIO_ILEDBOARD_H


#include "Arduino.h"


namespace Frangitron {

    class ILEDBoard {
    public:
        virtual ~ILEDBoard() {}

        virtual void displayWrite(uint8_t row, uint8_t column, String text) = 0;

        virtual const void* getIllumination() = 0;
        virtual void setIllumination(const void* illumination1) = 0;

        virtual const void* getSettings() = 0;
        virtual void setSettings(const void* settings1) = 0;

        virtual void setMappingTreeStructure(const void* mappingTreeStructure1) = 0;
        virtual void updateMappingTree(const void* mappingTreeLeaf1) = 0;

        virtual void loadSettings() = 0;
        virtual void saveSettings() = 0;

        virtual void resetMappingTree() = 0;
        virtual void loadMappingTree() = 0;
        virtual void saveMappingTree() = 0;
    };

}

#endif //PLATFORMIO_ILEDBOARD_H
