#pragma once
#include <stdint.h>
#include <limits.h>
// Legacy GGPO delta-only protocol: more:1, on:1, button:9, terminated by more=0.
inline bool GGPOInputEncodingValid(const uint8_t* bits,int numBits,int inputSize,uint32_t startFrame,int* frameCount=nullptr) {
    if(!bits || numBits<=0 || numBits>4096*8 || inputSize<=0 || inputSize>64 || startFrame>uint32_t(INT_MAX-1))return false;
    int offset=0,frames=0;
    auto read=[&](int count){int value=0;for(int i=0;i<count;++i,++offset)value|=((bits[offset/8]>>(offset%8))&1)<<i;return value;};
    while(offset<numBits) {
        while(read(1)) {
            if(numBits-offset<10)return false;
            read(1);if(read(9)>=inputSize*8)return false;
            if(offset>=numBits)return false;
        }
        ++frames;
    }
    if(uint64_t(startFrame)+uint64_t(frames)>uint64_t(INT_MAX))return false;
    if(frameCount)*frameCount=frames;return true;
}
