#include "storage/NativeEggInventory.hpp"
#include <cassert>
#include <climits>
using namespace Pokerogue3DS;
int main() {
    EggIncubationRecord eggs[2]{};eggs[0].id=3;eggs[1].id=8;
    eggs[0].hatchWaves=INT_MIN;eggs[0].timestamp=UINT64_MAX;
    eggs[0].variantTier=VariantTier::EPIC;eggs[0].isShiny=true;
    eggs[0].eggMoveIndex=3;eggs[0].overrideHiddenAbility=true;
    char bytes[60]{},second[60]{};size_t written=99;
    assert(encodeNativeEggInventory(eggs,2,bytes,sizeof(bytes),written)==NativeSaveResult::Ok && written==60);
    size_t same=0;assert(encodeNativeEggInventory(eggs,2,second,sizeof(second),same)==NativeSaveResult::Ok);
    assert(!std::memcmp(bytes,second,written));
    EggIncubationRecord decoded[2]{};size_t count=99;
    assert(decodeNativeEggInventory(bytes,written,decoded,2,count)==NativeSaveResult::Ok && count==2);
    assert(decoded[0].hatchWaves==INT_MIN && decoded[0].timestamp==UINT64_MAX
        && decoded[0].variantTier==VariantTier::EPIC && decoded[0].isShiny
        && decoded[0].eggMoveIndex==3 && decoded[0].overrideHiddenAbility);
    decoded[0].id=55;count=99;
    assert(decodeNativeEggInventory(bytes,written,decoded,1,count)==NativeSaveResult::TooLarge && decoded[0].id==55 && count==99);
    bytes[35]=1;assert(decodeNativeEggInventory(bytes,written,decoded,2,count)==NativeSaveResult::InvalidRecord && decoded[0].id==55);
    bytes[35]=0;bytes[33]=4;assert(inspectNativeEggInventory(bytes,written,count)==NativeSaveResult::InvalidRecord);
    bytes[33]=3;assert(inspectNativeEggInventory(bytes,written-1,count)==NativeSaveResult::InvalidFormat);
    eggs[1].id=3;written=99;
    assert(encodeNativeEggInventory(eggs,2,bytes,sizeof(bytes),written)==NativeSaveResult::InvalidRecord && written==99);
    assert(encodeNativeEggInventory(nullptr,0,bytes,sizeof(bytes),written)==NativeSaveResult::Ok && written==12);
    assert(decodeNativeEggInventory(bytes,written,nullptr,0,count)==NativeSaveResult::Ok && count==0);
}
