// This file was created by Filewrap 1.2
// Little endian mode
// DO NOT EDIT

#include "../PVRTMemoryFileSystem.h"

// using 32 bit to guarantee alignment.
#ifndef A32BIT
 #define A32BIT static const unsigned int
#endif

// ******** Start: texture.pvr ********

// File data
A32BIT _texture_pvr[] = {
0x3525650,0x0,0x1,0x0,0x0,0x0,0x10,0x10,0x1,0x1,0x1,0x5,0xf,0x3525650,0x3,0x3,0xff000200,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,0xffffffff,0xfeffffff,
0xffffff,
};

// Register texture.pvr in memory file system at application startup time
static CPVRTMemoryFileSystem RegisterFile_texture_pvr("texture.pvr", _texture_pvr, 259);

// ******** End: texture.pvr ********

