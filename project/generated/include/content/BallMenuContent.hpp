// Generated getPokeballAtlasKey/getPokeballName with parsed upstream enum IDs.
#pragma once
#include <cstdint>
namespace Pokerogue3DS {
struct BallMenuDefinition {uint8_t id;const char* iconKey;const char* label;const char* catchRateLabel;};
inline constexpr BallMenuDefinition kBallMenuDefinitions[]={
    {0,"pb","Poké Ball","1x"},
    {1,"gb","Super Ball","1.5x"},
    {2,"ub","Ultra Ball","2x"},
    {3,"rb","Rogue Ball","3x"},
    {4,"mb","Master Ball","100%"},
};
}
