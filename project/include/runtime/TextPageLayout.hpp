#pragma once
#include "runtime/Utf8Abbreviation.hpp"
#include <cstring>
namespace Pokerogue3DS {
struct TextPageLayout {
    char lines[12][256]{};
    unsigned lineCount=0;
    std::size_t consumed=0;
    bool valid=true,complete=false;
};
// A bounded page, measured with the selected native font. Never splits UTF-8.
template<class Measure> TextPageLayout layoutTextPage(const char* text,float maxWidth,Measure measure,unsigned maxLines=2) {
    TextPageLayout page;
    if(!text || !std::isfinite(maxWidth) || maxWidth<=0 || !maxLines || maxLines>12) {page.valid=false;return page;}
    std::size_t position=0;
    while(text[position] && page.lineCount<maxLines) {
        while(text[position]==' ' || text[position]=='\t') ++position;
        if(!text[position]) break;
        char* line=page.lines[page.lineCount++];
        const std::size_t start=position;std::size_t length=0,breakEnd=0,breakInk=0;
        while(text[position]) {
            uint32_t cp=0;const unsigned n=utf8CodePoint(text+position,cp);
            if(!n) {page.valid=false;return page;}
            if(cp=='\n' || cp=='\r') {
                position+=n;
                if(cp=='\r' && text[position]=='\n') ++position;
                break;
            }
            bool fits=false;
            if(length+n<sizeof(page.lines[0])) {
                std::memcpy(line+length,text+position,n);line[length+n]=0;
                const float width=measure(line);
                if(!std::isfinite(width) || width<0) {page.valid=false;return page;}
                fits=width<=maxWidth;
            }
            if(!fits) {
                line[length]=0;
                if(!length) {page.valid=false;return page;}
                if(breakEnd) {line[breakInk]=0;position=start+breakEnd;}
                break;
            }
            if(cp==' ' || cp=='\t') {breakInk=length;breakEnd=length+n;}
            length+=n;position+=n;
        }
    }
    page.consumed=position;page.complete=!text[position];return page;
}
}
