#ifndef FIRMWARE_FULL_MAPPINGTREE_H
#define FIRMWARE_FULL_MAPPINGTREE_H

#include <vector>

namespace Frangitron {

    struct MappingTree {
        std::vector<int> universeA[128];
        std::vector<int> universeB[128];
        std::vector<int> universeC[128];
    };

}

#endif //FIRMWARE_FULL_MAPPINGTREE_H
