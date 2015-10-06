libgfx
======


*libgfx* is a free library for handling graphics files, especially for games.

There are many graphics file formats, programmers can use for they 2D games,
each one with its pros and cons. The most popular file format propably is
**PNG**, as it is free to used and supported by almost any software and libraries.

One issue most programmers of 2D games are faced with is how to store
additional, game-related data? Sprites often have animations, bitmap fonts
have any dimensions etc., and there are several ways to solve that problem:

 -# **Hard-code** it in the game engine - hard to maintain, because the engine
    has to be recompiled if something changed with the graphics
 -# Use seperated **meta files** storing the additional data - you need 2 files
    per graphic (or 1 global additional file for all graphics)
 -# Implement a simple **custom file format** - not using the advantages of other
    file formats (like the good compression of PNG), unless you are a very
    good programmer

But, if you take a look to the specification of the PNG file format, you'll
see that it *officially* supports custom, "private" user data. The PNG format
is based on chunks, usually public chunks (image metadata, pixels data, palette, etc.),
but could have private chunks as well.

The *libpng* API is quite difficult to use, and most higher-level libraries,
allowing you to read PNG, does not allow you to read custom chunks or even
write PNG files with custom chunks. This is why I've been working on a
library to make it possible to you: **libgfx**.

With *libgfx* you can read and write PNG files, with or without custom chunks,
and quite any format you wish (RGB, RGBA, palette, etc.).


Compiling
---------

The source code consists of two major files: include/libgfx.h containing the
definitions for including in your source and src/libgfx.cpp providing the
implementation. This library depends on libpng, so make sure it is already
installed and can be found with `pkg-config`. If needed, edit the Makefile to
fit your environment.

In a unix environment it should be enough to call `make` and `make install`.
It installs libgfx.a and libgfx.h, the generated documentation and PC file for
pkg-config in `/usr/local` by default.


Reading PNG File
----------------

~~~~~~~~
#include <libgfx.h>

libgfx_image img;
libgfx_loadGfxFile(&img, "yourfile.png");
/* now you can check bitDepth and colorFormat
 * and create your texture using img.pixels */
~~~~~~~~


Writing PNG File
----------------

~~~~~~~~
#include <libgfx.h>
...
libgfx_image img;
/* Set your image data in img.pixels, img.width and img.height.
 * Optionally give your image a palette and userChunks.
 * And don't to set bitDepth and colorFormat.
 */
img.pixels = ...;
img.width = 64;
img.height=  64;
img.bitDepth = 8;
img.colorFormat = GfxFormat_RGBA;
libgfx_writeGfxFile(&img, "yourfile.png");
~~~~~~~~


Using Callbacks
---------------

~~~~~~~~
#include <libgfx.h>

void myWriteFunction(const void *ptr, int size, void *user)
{
  // write size bytes, using your passed user pointer if necesarry
}

void myReadFunction(void *ptr, int size, void *user)
{
  // read size bytes (or less) and write them into ptr
}

...
libgfx_writeGfx(&img, myWriteFunction, fp);
...
libgfx_readGfx(&img, myReadFunction, fp);
~~~~~~~~


Using Custom Chunks
-------------------

Please refer to GfxChunk for more details.

~~~~~~~~
#include <libgfx.h>

...

libgfx_img img;
GfxChunk myChunk;
strcpy(myChunk.name, "bbOX");
myChunk.data.resize(4);
myChunk.data[0] = bbox.left;
myChunk.data[1] = bbox.top;
myChunk.data[2] = bbox.width;
myChunk.data[3] = bbox.height;
img.userChunks.push_back(&myChunk);
...
libgfx_writeGfxFile(&img, "test.png");

...

libgfx_readGfxFile(&img, "test.png");
GfxChunk *myChunk = img.userChunks[0];
bbox.left = myChunk->data[0];
bbox.top = myChunk->data[1];
bbox.width = myChunk->data[2];
bbox.height = myChunk->data[3];
~~~~~~~~
