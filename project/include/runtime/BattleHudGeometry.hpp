#pragma once
#include <cmath>
#include <cstdint>
namespace Pokerogue3DS {
// EnemyBattleInfo.updateBossSegmentDividers, quantized to native pixel columns.
inline unsigned bossDividerPixel(uint32_t maxHp,unsigned segments,unsigned boundary,unsigned width) {
    if(!maxHp || segments<2 || !boundary || boundary>=segments || !width) return 0;
    const double ratio=std::round((double(maxHp)/segments)*boundary)/maxHp;
    const double pixel=std::round(ratio*width-0.5);
    return pixel<0 ? 0 : (pixel>=width ? width-1 : static_cast<unsigned>(pixel));
}
}
