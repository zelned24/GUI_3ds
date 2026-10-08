#pragma once
#include <cmath>
namespace Pokerogue3DS {
inline constexpr unsigned kNativeFontPoints[]={8,10,12,16};
struct NativeTextRaster {unsigned index=3;unsigned scale=1;float authoredSize=0.5f;};
// The pinned Emerald font has complete strokes at the 12-point raster
// (16 source pixels at 96 DPI). The 8/10-point outlines lose strokes before
// reaching the GPU. Use that legible source and whole enlargement multiples.
inline constexpr unsigned kNativeLegibleFontIndex=2;
inline NativeTextRaster nativeTextRaster(float size) {
    NativeTextRaster result{kNativeLegibleFontIndex,1,0.375f};
    if(!std::isfinite(size) || size<=0) return result;
    const float wanted=size*32;
    float error=INFINITY;
    for(unsigned scale=1;scale<=8;++scale) {
        const float candidate=float(kNativeFontPoints[kNativeLegibleFontIndex]*scale);
        const float difference=std::fabs(candidate-wanted);
        if(difference<error) {
            error=difference;result={kNativeLegibleFontIndex,scale,candidate/32};
        }
    }
    return result;
}
}
