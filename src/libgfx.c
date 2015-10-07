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

#include <png.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>

#include "libgfx.h"


typedef struct
{
  void *userData;
  void *userFn;
} _userCb;


static void _userWriteData(png_structp png_ptr, png_bytep data, png_size_t length)
{
  _userCb *userCb = (_userCb*)png_get_io_ptr(png_ptr);
  ((GfxWriteCb)userCb->userFn)(data, length, userCb->userData);
}

static void _userReadData(png_structp png_ptr, png_bytep data, png_size_t length)
{
  _userCb *userCb = (_userCb*)png_get_io_ptr(png_ptr);
  ((GfxReadCb)userCb->userFn)(data, length, userCb->userData);
}


int libgfx_createImage(GfxImage *img)
{
  switch(img->colorFormat)
  {
    case GfxFormat_RGB:
      img->pixels = malloc(img->width * img->height * 3 * (img->bitDepth / 8));
      break;
    case GfxFormat_RGBA:
      img->pixels = malloc(img->width * img->height * 4 * (img->bitDepth / 8));
      break;
    case GfxFormat_Palette:
      img->pixels = malloc(img->width * img->height * (img->bitDepth / 8));
      break;
  }
  
  img->palette = 0;
  img->nColors = 0;
  img->userChunks = 0;
  img->nUserChunks = 0;
  
  return GfxError_None;
}

int libgfx_destroyImage(GfxImage *img)
{
  if(img->palette != 0) free(img->palette);
  if(img->userChunks != 0) free(img->userChunks);
  if(img->pixels != 0) free(img->pixels);
  
  return GfxError_None;
}

int libgfx_createPalette(GfxImage *img, int nEntries)
{
  img->nColors = nEntries;
  img->palette = calloc(nEntries * 4, img->bitDepth / 8);
  
  return GfxError_None;
}

int libgfx_createChunk(GfxImage *img, const char *name, size_t size)
{
  img->userChunks = realloc(img->userChunks, sizeof(GfxChunk*) * ++img->nUserChunks);
  
  GfxChunk *chunk = malloc(sizeof(GfxChunk));
  strncpy(chunk->name, name, 5);
  chunk->size = size;
  chunk->data = malloc(size);
  
  return GfxError_None;
}

int libgfx_writeGfx(GfxImage *img, GfxWriteCb writeFn, void *user)
{
  GfxError err = GfxError_None;
  /* user callback initialization */
  _userCb userCb;
  userCb.userData = user;
  userCb.userFn = (void*)writeFn;
  
  /* png structs initialization */
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  png_infop info_ptr = 0;
  if(!png_ptr) return GfxError_Unknown;
  
  
  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr)
  {
    err = GfxError_Unknown;
    goto exit;
  }
  
  /* let png know about our user function */
  png_set_write_fn(png_ptr, &userCb, _userWriteData, 0);
  
  if(img->fileFormat != GfxFormat_Png)
  {
    /* custom GFX format */
    png_set_sig_bytes(png_ptr, 8);
    const char sig[] = "AGFX";
    ((GfxWriteCb)userCb.userFn)(sig, 5, userCb.userData);
  }
  
  
  /* tell png about custom user chunks */
  png_unknown_chunkp chunks = malloc(img->nUserChunks);
  size_t i;
  for(i = 0; i < img->nUserChunks; ++i)
  {
    GfxChunk *chunk = img->userChunks[i];
    png_unknown_chunk ch;
    memcpy(ch.name, chunk->name, 4);
    ch.name[4] = 0x00;
    ch.data = chunk->data;
    ch.size = chunk->size;
    ch.location = PNG_AFTER_IDAT;
    
    chunks[i] = ch;
  }
  /* name, data, size, location */
  png_set_unknown_chunks(png_ptr, info_ptr, chunks, img->nUserChunks);
  /* keep unsafe-to-copy chunks */
  png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS, 0, 0);
  
  
  
  int bit_depth, color_type, bits_per_pixel;
  switch(img->colorFormat)
  {
    default:
      err = GfxError_ColorFormat;
      goto exit;
    case GfxFormat_RGB:
      color_type = PNG_COLOR_TYPE_RGB;
      bit_depth = 8;
      bits_per_pixel = 24;
      break;
    case GfxFormat_RGBA:
      color_type = PNG_COLOR_TYPE_RGB_ALPHA;
      bit_depth = 8;
      bits_per_pixel = 32;
      break;
    case GfxFormat_Palette:
      color_type = PNG_COLOR_TYPE_PALETTE;
      if(img->bitDepth <= 4)
      {
        bit_depth = 4;
        bits_per_pixel = 4;
      }
      else
      {
        bit_depth = 8;
        bits_per_pixel = 8;
      }
      break;
  }
  
  png_set_IHDR(png_ptr, info_ptr, img->width, img->height, bit_depth, color_type,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  
  
  if(img->colorFormat == GfxFormat_Palette)
  {
    int nColors = img->nColors;
    if(bit_depth == 4 && nColors > 16) nColors = 16;
    png_color palette[nColors];
    size_t i;
    for(i = 0; i < nColors; ++i)
    {
      palette[i].red = img->palette[i * 3];
      palette[i].green = img->palette[i * 3 + 1];
      palette[i].blue = img->palette[i * 3 + 2];
    }
    png_set_PLTE(png_ptr, info_ptr, palette, nColors);
  }
  
  
  
  int bytes_per_row = img->width * ((bits_per_pixel + 7) / 8);
  png_bytepp row_pointers = malloc(sizeof(png_bytep) * img->height);
  int y;
  for(y = 0; y < img->height; ++y)
  {
    /* row_pointers[y] = new png_byte[bytes_per_row]; */
    row_pointers[y] = malloc(sizeof(png_byte) * bytes_per_row);
    memcpy(row_pointers[y], &(img->pixels[y * bytes_per_row]), bytes_per_row);
  }
  png_set_rows(png_ptr, info_ptr, row_pointers);
  png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, 0);
  
  for(y = 0; y < img->height; ++y)
  {
    /* delete row_pointers[y]; */
    free(row_pointers[y]);
  }
  free(row_pointers);

  exit:
  png_destroy_write_struct(&png_ptr, &info_ptr);
  
  return err;
}

static void _writeFn(const void *ptr, int size, void *user)
{
  FILE *fp = (FILE*)user;
  fwrite(ptr, 1, size, fp);
}
int libgfx_writeGfxFile(GfxImage *img, const char *fname)
{
  FILE *fp = fopen(fname, "wb");
  if(!fp) return GfxError_File;
  
  int ret = libgfx_writeGfx(img, _writeFn, fp);
  fclose(fp);
  
  return ret;
}

int libgfx_loadGfx(GfxImage *img, GfxReadCb readFn, void *user)
{
  /* user callback initialization */
  GfxError err = GfxError_None;
  
  _userCb userCb;
  userCb.userData = user;
  userCb.userFn = (void*)readFn;
  
  /* png structs initialization */
  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  png_infop info_ptr = 0;
  if(!png_ptr) return GfxError_Unknown;
  
  
  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr) { err = GfxError_Unknown; goto exit; }
  
  
  /* let png know about our user function */
  png_set_read_fn(png_ptr, &userCb, _userReadData);
  
  
  char header[8];
  ((GfxReadCb)userCb.userFn)(header, 5, userCb.userData);
  if(strncmp(header, "AGFX", 4))
  {
    ((GfxReadCb)userCb.userFn)(&header[5], 3, userCb.userData);
    if(png_sig_cmp((png_bytep)header, 0, 8))
    {
      /* TODO: is it really AGFX? */
      img->fileFormat = GfxFormat_Gfx;
      err = GfxError_FileFormat; /* not yet supported */
      goto exit;
    }
  }
  png_set_sig_bytes(png_ptr, 8);
  
  
  png_set_keep_unknown_chunks(png_ptr, 3, 0, -1); /* keep all chunks */
  
  png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, 0);
  
  png_unknown_chunkp chunks;
  int nChunks = png_get_unknown_chunks(png_ptr, info_ptr, &chunks);
  
  /* img->userChunks.reserve(nChunks); */
  size_t i;
  for(i = 0; i < nChunks; ++i)
  {
    GfxChunk *userChunk;
    libgfx_createChunk(img, chunks[i].name, chunks[i].size);
    userChunk = img->userChunks[img->nUserChunks - 1];
    /* GfxChunk *userChunk = new GfxChunk; */
    /* memcpy(userChunk->name, chunks[i].name, 4);
    userChunk->name[4] = 0x00;
    userChunk->data.resize(chunks[i].size); */
    memcpy(userChunk->data, chunks[i].data, chunks[i].size);
    /* img->userChunks.push_back(userChunk); */
  }
  
  
  img->width = png_get_image_width(png_ptr, info_ptr);
  img->height = png_get_image_height(png_ptr, info_ptr);
  /* img->pixels.resize(img->width * img->height * 4); */
  
  
  
  int color_type = png_get_color_type(png_ptr, info_ptr);
  switch(color_type)
  {
    default:
      err = GfxError_ColorFormat;
      goto exit;
    case PNG_COLOR_TYPE_PALETTE:
    {
      /* palette is internally converted into rgba */
      img->colorFormat = GfxFormat_RGBA;
      
      png_colorp palette;
      int nColors;
      png_get_PLTE(png_ptr, info_ptr, &palette, &nColors);
      png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
      int bit_depth = img->bitDepth = png_get_bit_depth(png_ptr, info_ptr);
      
      libgfx_createImage(img);
      GfxRGBA8 *pixels = (GfxRGBA8*) (img->pixels);
      
      png_bytep trans;
      int num_trans;
      /* png_color_16p trans_values; // values */
      png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, 0);
      
      if(bit_depth == 8)
      {
        int i;
        for(i = 0; i < img->width * img->height; ++i)
        {
          int x = i % img->width;
          int y = i / img->width;
          
          int ind = row_pointers[y][x];
          png_color col = palette[ind];
          
          pixels[i].red = col.red;
          pixels[i].green = col.green;
          pixels[i].blue = col.blue;
          /* pixels[i].alpha = (ind == 0) ? 0 : 255; */
          if(trans && ind < num_trans) pixels[i].alpha = trans[ind];
          else pixels[i].alpha = 255;
        }
      }
      else if(bit_depth == 4)
      {
        int i;
        for(i = 0; i < img->width * img->height; i += 2)
        {
          int x = i % img->width;
          int y = i / img->width;
          
          int ind = (row_pointers[y][x / 2] >> 4) & 0xF;
          png_color col = palette[ind];
          
          pixels[i].red = col.red;
          pixels[i].green = col.green;
          pixels[i].blue = col.blue;
          /* pixels[i].alpha = (ind == 0) ? 0 : 255; */
          if(trans && ind < num_trans) pixels[i].alpha = trans[ind];
          else pixels[i].alpha = 255;
          
          
          ind = row_pointers[y][x / 2] & 0xF;
          col = palette[ind];
          
          pixels[i + 1].red = col.red;
          pixels[i + 1].green = col.green;
          pixels[i + 1].blue = col.blue;
          pixels[i + 1].alpha = (ind == 0) ? 0 : 255;
        }
      }
      else
      {
        err = GfxError_BitDepth;
        goto exit;
      }
      break;
    }
    case PNG_COLOR_TYPE_RGB: case PNG_COLOR_TYPE_RGB_ALPHA:
    {
      /*if(color_type == PNG_COLOR_TYPE_RGB_ALPHA) img->colorFormat = GfxFormat_RGBA;
      else img->colorFormat = GfxFormat_RGB;*/
      img->colorFormat = GfxFormat_RGBA;
      
      int bit_depth = img->bitDepth = png_get_bit_depth(png_ptr, info_ptr); // assuming 8
      if(bit_depth != 8)
      {
        err = GfxError_ColorFormat;
        goto exit;
      }
      png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
      
      libgfx_createImage(img);
      GfxRGBA8 *pixels = (GfxRGBA8*) (img->pixels);
      
      int i;
      for(i = 0; i < img->width * img->height; ++i)
      {
        int x = i % img->width;
        int y = i / img->width;
        int bpp = (color_type == PNG_COLOR_TYPE_RGB_ALPHA) ? 4 : 3;
        GfxRGBA8 *col = (GfxRGBA8*) (&row_pointers[y][x * bpp]);
        
        pixels[i].red = col->red;
        pixels[i].green = col->green;
        pixels[i].blue = col->blue;
        pixels[i].alpha = (bpp == 4) ? col->alpha : 255;
      }
      
      break;
    }
  }
  
  exit:
  png_destroy_read_struct(&png_ptr, &info_ptr, 0);
  
  return err;
}

static void _readFn(void *ptr, int size, void *user)
{
  FILE *fp = (FILE*)user;
  fread(ptr, 1, size, fp);
}
int libgfx_loadGfxFile(GfxImage *img, const char *fname)
{
  FILE *fp = fopen(fname, "rb");
  if(!fp) return GfxError_File;
  
  int ret = libgfx_loadGfx(img, _readFn, fp);
  fclose(fp);
  
  return ret;
}
