#pragma once
#include <cmath>
namespace Pokerogue3DS {
inline constexpr unsigned kNativeFontPoints[]={8,10,12,16};
struct NativeTextRaster {unsigned index=3;unsigned scale=1;float authoredSize=0.5f;};
// Existing presentation sizes target 32 points. Rasterize once at the closest
// native size and draw only whole multiples; choose the larger source on ties.
inline NativeTextRaster nativeTextRaster(float size) {
    NativeTextRaster result;
    if(!std::isfinite(size) || size<=0) return result;
    const float wanted=size*32;
    float error=INFINITY;
    for(unsigned i=0;i<4;++i) for(unsigned scale=1;scale<=8;++scale) {
        const float candidate=float(kNativeFontPoints[i]*scale);
        const float difference=std::fabs(candidate-wanted);
        if(difference<error || (difference==error && i>result.index)) {
            error=difference;result={i,scale,candidate/32};
        }
    }
    return result;
}
}
