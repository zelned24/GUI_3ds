#include "runtime/PokemonAtlasMetadata.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <string>
#include <cstdio>
#include "content/NativeSpritePolicy.hpp"
using namespace Pokerogue3DS;
int main(int argc,char** argv) {
    assert(argc==3);
    if(std::strcmp(argv[1],"--catalog")==0) {
        std::ifstream paths(argv[2]);assert(paths.good());
        std::string path;unsigned atlases=0,frames=0;
        PokemonAtlasMetadata atlas;
        while(std::getline(paths,path)) {
            assert(atlas.load(path.c_str()));
            const bool back=path.find("/atlas/back/")!=std::string::npos;
            assert(atlas.canvasWidth()<=(back ? kNativeBackCanvasWidth : kNativeFrontCanvasWidth));
            assert(atlas.canvasHeight()<=(back ? kNativeBackCanvasHeight : kNativeFrontCanvasHeight));
            for(unsigned i=0;i<atlas.frameCount();++i) assert(atlas.frame(i)->page()<4);
            assert(atlas.animationFrame(0) && atlas.animationFrame(39900));
            ++atlases;frames+=atlas.frameCount();
        }
        assert(paths.eof() && atlases>0);
        std::printf("PASS C++ runtime parser: %u production atlases, %u frames\n",atlases,frames);
        return 0;
    }
    PokemonAtlasMetadata atlas;
    assert(atlas.load(argv[1]));
    assert(!atlas.paged() && atlas.frameCount()==2);
    assert(atlas.canvasWidth()==13 && atlas.canvasHeight()==8);
    assert(std::strcmp(atlas.animationFrame(0)->filename,"0001.png")==0);
    assert(std::strcmp(atlas.animationFrame(100)->filename,"0002.png")==0);
    assert(std::strcmp(atlas.animationFrame(200)->filename,"0001.png")==0);
    assert(std::strcmp(atlas.animationFrame(41,24)->filename,"0001.png")==0);
    assert(std::strcmp(atlas.animationFrame(42,24)->filename,"0002.png")==0);
    assert(std::strcmp(atlas.animationFrame(84,24)->filename,"0001.png")==0);
    assert(!atlas.animationFrame(0,0) && !atlas.animationFrame(0,1001));
    assert(atlas.animationFrame(UINT64_MAX,24));
    assert(std::strcmp(atlas.animationFrame(42,24,1)->filename,"0001.png")==0);
    assert(!atlas.animationFrame(0,24,0) && !atlas.animationFrame(0,24,401));
    assert(!atlas.frame(2) && !atlas.find("missing.png"));
    unsigned char badHash[32]={1};
    assert(!atlas.load(argv[1],badHash));
    assert(atlas.canvasWidth()==0 && atlas.canvasHeight()==0 && atlas.frameCount()==0);
    assert(atlas.load(argv[2]));
    assert(atlas.paged() && atlas.frameCount()==2);
    assert(atlas.canvasWidth()==6 && atlas.canvasHeight()==3);
    assert(atlas.frame(0)->page()==0);
    atlas.clear();
    assert(!atlas.animationFrame(0) && !atlas.paged() && atlas.width()==0 && atlas.height()==0);
    assert(atlas.canvasWidth()==0 && atlas.canvasHeight()==0);
    assert(!atlas.load(nullptr));
}
