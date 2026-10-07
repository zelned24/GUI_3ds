#include "runtime/TextPageLayout.hpp"
#include <cassert>
#include <string>
#include <limits>
using namespace Pokerogue3DS;
int main() {
    auto measure=[](const char* text) {unsigned count=0;for(const char* p=text;*p;) {uint32_t cp;const unsigned n=utf8CodePoint(p,cp);assert(n);p+=n;++count;}return float(count);};
    auto page=layoutTextPage("hello world again",5,measure);
    assert(page.valid && !page.complete && page.lineCount==2);
    assert(std::string(page.lines[0])=="hello");
    assert(std::string(page.lines[1])=="world" && page.consumed==11);
    // Boundary spaces remain part of the consumed stream, never its words.
    std::string text="Pok\xc3\xa9mon usa un movimiento muy largo y eficaz. "+std::string(1500,'A');
    std::string recovered;std::size_t offset=0;unsigned pages=0;
    while(offset<text.size()) {
        page=layoutTextPage(text.c_str()+offset,17,measure);
        assert(page.valid && page.consumed>0 && page.consumed<=text.size()-offset);
        for(unsigned i=0;i<page.lineCount;++i) {assert(measure(page.lines[i])<=17);recovered+=page.lines[i];}
        offset+=page.consumed;++pages;assert(pages<200);
    }
    auto withoutSpaces=[](std::string s) {std::string out;for(char c:s) if(c!=' ' && c!='\t') out+=c;return out;};
    assert(withoutSpaces(recovered)==withoutSpaces(text));
    page=layoutTextPage("a\r\nb\nc",5,measure);
    assert(std::string(page.lines[0])=="a" && std::string(page.lines[1])=="b" && page.consumed==5);
    assert(!page.complete);
    page=layoutTextPage("",5,measure);assert(page.valid && page.complete && !page.lineCount);
    assert(!layoutTextPage("A",0.5f,measure).valid);
    assert(!layoutTextPage(nullptr,5,measure).valid);
    assert(!layoutTextPage("A",0,measure).valid);
    assert(!layoutTextPage("A",5,measure,0).valid);
    assert(!layoutTextPage("A",5,measure,13).valid);
    assert(!layoutTextPage("A",5,[](const char*) {return std::numeric_limits<float>::quiet_NaN();}).valid);
    assert(!layoutTextPage("\xc3",5,measure).valid);
    page=layoutTextPage(std::string(800,'A').c_str(),1000,measure);
    assert(page.valid && !page.complete && page.consumed==510);
    page=layoutTextPage(std::string(1500,'A').c_str(),17,measure,12);
    assert(page.valid && page.lineCount==12 && page.consumed==204);
}
