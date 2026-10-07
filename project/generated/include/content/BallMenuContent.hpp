// Generated getPokeballAtlasKey/getPokeballName with parsed upstream enum IDs.
#pragma once
#include <cstdint>
namespace Pokerogue3DS {
struct BallMenuDefinition {uint8_t id;const char* iconKey;const char* label;};
inline constexpr BallMenuDefinition kBallMenuDefinitions[]={
    {0,"pb","Poké Ball"},
    {1,"gb","Super Ball"},
    {2,"ub","Ultra Ball"},
    {3,"rb","Rogue Ball"},
    {4,"mb","Master Ball"},
};
}
