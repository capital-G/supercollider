#include "SC_InterfaceTable.h"
#include "SC_Unit.h"
#include "SC_PlugIn.hpp"

static InterfaceTable* ft;

/** @brief helper method to extract a string from a float signal.
 *  The caller is transferred ownership of the realtime char, which will return
 *  a nullptr if allocation failed.
 *
 *  A float can store precisely up to 2**24 + 1 integers.
 *  We can therefore use these 24 bits to transport a series of UTF-8 chars.
 *  The first byte of the signal is expected to determine the number of chars
 *  (limiting it to 256 chars), while the following bytes are expected to be encoded as ASCII (potentially utf-8).
 *  There will be as many chars consumed from the consecutive float array as numChars are stated.
 *
 *  @note We do not use the full 32 bit of a float as it is unclear
 *  if scsynth/supernova passes nan floats transparently.
 *
 */
char* floatStringDecoder(World* world, float** signals) {
    int32 currentData = static_cast<int32>(*signals[0]);
    // extract first byte of currentData which stores the length
    int32 length = currentData & 0xff;
    if (length == 0)
        return nullptr;

    char* str = static_cast<char*>(RTAlloc(world, length + 1));
    if (!str)
        return nullptr;

    int signalIndex = 0;
    // first byte is length, so we skip that
    int byteOffset = 1;
    currentData >>= 8;
    for (int i = 0; i < length; i++) {
        if (byteOffset % 3 == 0) {
            signalIndex += 1;
            currentData = static_cast<int32>(*signals[signalIndex]);
        }
        str[i] = static_cast<char>(currentData & 0xff);
        byteOffset += 1;
        currentData >>= 8;
    }
    str[length] = '\0';

    return str;
}

class SynthSpawn : public SCUnit {
public:
    SynthSpawn() {
        mSynthName = floatStringDecoder(mWorld, mInBuf + 4);
        set_calc_function<SynthSpawn, &SynthSpawn::next>();
    };
    ~SynthSpawn() { RTFree(mWorld, mSynthName); };

    // rt managed!
    // it is fine to delete this upon destruction b/c this string is only referenced during scheduling,
    // beyond that the char has been resolved to a SynthDef reference.
    char* mSynthName;

private:
    void next(int numSamples) {
        for (int i = 0; i < numSamples; i++) {
            if (in(0)[i] > 0.0f) {
                ft->fSpawnSynth(mWorld, mSynthName, in(1)[i], in(2)[i], in(3)[i], i);
            }
            out(0)[i] = in(0)[i];
        }
    };
};


PluginLoad(MetaUGen) {
    ft = inTable;
    registerUnit<SynthSpawn>(ft, "SpawnSynth", false);
}
