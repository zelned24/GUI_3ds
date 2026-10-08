#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>
namespace Pokerogue3DS {
inline unsigned utf8CodePoint(const char* p,uint32_t& cp) {
    const auto a=static_cast<unsigned char>(*p);if(!a) return 0;
    unsigned n=1;cp=a;
    if(a>=0xc2 && a<=0xdf) {n=2;cp=a&31;}
    else if(a>=0xe0 && a<=0xef) {n=3;cp=a&15;}
    else if(a>=0xf0 && a<=0xf4) {n=4;cp=a&7;}
    else if(a>=0x80) return 0;
    for(unsigned i=1;i<n;++i) {const auto b=static_cast<unsigned char>(p[i]);if((b&0xc0)!=0x80) return 0;cp=(cp<<6)|(b&63);}
    if((n==2 && cp<0x80) || (n==3 && cp<0x800) || (n==4 && cp<0x10000) || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return 0;
    return n;
}
inline std::size_t previousUtf8(const char* text,std::size_t end) {
    if(!end) return 0;
    --end;
    while(end && (static_cast<unsigned char>(text[end])&0xc0)==0x80) --end;
    return end;
}
inline bool jsTrailingSpace(uint32_t cp) {
    return (cp>=9 && cp<=13) || cp==32 || cp==0xa0 || cp==0x1680 || (cp>=0x2000 && cp<=0x200a) || cp==0x2028 || cp==0x2029 || cp==0x202f || cp==0x205f || cp==0x3000 || cp==0xfeff;
}
// BattleInfo.updateNameText: strip gender and abbreviate with '.', keeping font scale.
// Measure owns its scratch storage; this routine never allocates or mutates source text.
template<class Measure> bool abbreviateUtf8(const char* source,char* out,std::size_t capacity,float maxWidth,bool stripGender,Measure measure,float& width) {
    width=0;if(!out || !capacity) return false;out[0]=0;if(capacity<2) return false;
    if(!source || !std::isfinite(maxWidth) || maxWidth<=0) return false;
    std::size_t length=0;bool overflow=false;
    for(const char* p=source;*p;) {
        uint32_t cp;const unsigned n=utf8CodePoint(p,cp);if(!n) {out[0]=0;return false;}
        if(!stripGender || (cp!=0x2640 && cp!=0x2642)) {
            if(!overflow && length+n<capacity) {for(unsigned i=0;i<n;++i) out[length++]=p[i];}
            else overflow=true;
        }
        p+=n;
    }
    out[length]=0;
    auto appendDot=[&]() {
        if(length && out[length-1]=='.') --length;
        else if(length) length=previousUtf8(out,length);
        while(length) {const auto start=previousUtf8(out,length);uint32_t cp=0;if(!utf8CodePoint(out+start,cp) || !jsTrailingSpace(cp)) break;length=start;}
        out[length++]='.';out[length]=0;
    };
    if(overflow) appendDot();
    while(true) {
        width=measure(out);
        if(!std::isfinite(width) || width<0) {out[0]=0;width=0;return false;}
        if(width<=maxWidth) return true;
        if(length<=1) {out[0]=0;width=0;return false;}
        // If an earlier iteration appended '.', remove one more full codepoint.
        if(out[length-1]=='.') {--length;if(length) length=previousUtf8(out,length);out[length]=0;}
        else length=previousUtf8(out,length);
        while(length) {const auto start=previousUtf8(out,length);uint32_t cp=0;if(!utf8CodePoint(out+start,cp) || !jsTrailingSpace(cp)) break;length=start;}
        out[length++]='.';out[length]=0;
    }
}
}
