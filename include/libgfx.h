/*

Copyright (c) 2015 Simon "Tenry" Burchert

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgement in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

*/

#ifndef _libgfx_h
#define _libgfx_h

/** @file */


#include <stdint.h>
#include <vector>


typedef void (*GfxWriteCb)(const void *ptr, int size, void *user);
typedef void (*GfxReadCb)(void *ptr, int size, void *user);


/** @brief Error codes that can be returned by the libgfx functions. */
enum GfxError
{
  GfxError_None=0,
  GfxError_Unknown,
  GfxError_ColorFormat, /**< @brief The specified color format is not supported */
  GfxError_File, /**< @brief Couldn't open specified file for reading or writing */
  GfxError_FileFormat, /**< @brief The specified file format is not supported */
  GfxError_BitDepth
};
enum GfxFileFormat
{
  GfxFormat_Png=1,
  GfxFormat_Gfx
};
/** @brief Color format. */
enum GfxColorFormat
{
  GfxFormat_RGB=1,
  GfxFormat_RGBA,
  GfxFormat_Palette
};


struct GfxRGB8
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} __attribute__((packed));

struct GfxRGBA8
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
} __attribute__((packed));

/** @brief Struct for user chunks. */
struct GfxChunk
{
  /** @brief Name of the chunk.
  
  Must consist of 4 letters, with the case of the letters having meanings:
  
  # | Uppercase | Lowercase | Notes
  --|-----------|-----------|-------
  1 | Critical | Not critical | Usually lowercase
  2 | Public | Private | Shall be lowercase
  3 |  | Invalid | Must be uppercase
  4 | Unsafe to copy | Safe to copy | Safe to copy means that the chunk can be kept if the image data changed
  
  The fifth byte should be zero.
  
  */
  char name[5];
  
  /** @brief The binary data of the chunk. */
  std::vector<uint8_t> data;
};


/** @brief Structure containing the raw image data. */
struct libgfx_image
{
  GfxFileFormat fileFormat;
  GfxColorFormat colorFormat;
  
  /** @brief Bits per channel.
  
  For GfxFormat_RGB and GfxFormat_RGBA, it is the number of bytes per
  channel (R, G, B and alpha) and shall be 8.
  
  For GfxFormat_Palette, it is the number of bytes per color index.
  Shall be 8 (1 byte per pixel) or 16 (2 bytes per pixel).
  
  */
  int bitDepth;
  
  /** @brief The image data to read or write. */
  std::vector<uint8_t> pixels;
  
  /** @brief The colors of the palette.
  
  **Reading**: When loading an image, that contains a palette, it is stored
  in this variable (even, if the user requested true colors).
  
  **Writing**: When writing an image, that shall have a palette, this must
  be filled with any available colors.
  
  */
  std::vector<uint8_t> palette;
  
  int width, height;
  
  std::vector<GfxChunk*> userChunks;
};


/** @brief Write a GFX file.

@param img The image data to write
@param writeFn The user write function
@param user The user data passed to the write function

@return 0 on success or an GfxError value on error.

@see libgfx_writeGfxFile()
@see libgfx_loadGfx()
*/
int libgfx_writeGfx(libgfx_image *img, GfxWriteCb writeFn, void *user=nullptr);

/** @brief Write a GFX file.

@param img The image data to write
@param fname The file name to be written

@return 0 on success or an GfxError value on error.

@see libgfx_writeGfx()
@see libgfx_loadGfxFile()
*/
int libgfx_writeGfxFile(libgfx_image *img, const char *fname);


/** @brief Load a GFX file.

@param img The image data to be filled
@param readFn The user read function
@param user The user data passed to the read function

@return 0 on success or an GfxError value on error.

@see libgfx_loadGfxFile()
@see libgfx_writeGfx()
*/
int libgfx_loadGfx(libgfx_image *img, GfxReadCb readFn, void *user=nullptr);

/** @brief Load a GFX file.

@param img The image data to be filled
@param fname The file name to be loaded

@return 0 on success or an GfxError value on error.

@see libgfx_loadGfx()
@see libgfx_writeGfxFile()
*/
int libgfx_loadGfxFile(libgfx_image *img, const char *fname);
int libgfx_loadGfxMem(libgfx_image *img, const void *ptr, int size);

/** @brief Unset user chunks.

This function shall be called after libgfx_loadGfx(), when the user chunks
are not needed any longer.
*/
void libgfx_clearChunks(libgfx_image *img);

#endif
