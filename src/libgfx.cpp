#include <png.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "libgfx.h"


struct _userCb
{
  void *userData;
  void *userFn;
};


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


int libgfx_writeGfx(libgfx_image *img, GfxWriteCb writeFn, void *user)
{
  /* user callback initialization */
  _userCb userCb;
  userCb.userData = user;
  userCb.userFn = (void*)writeFn;
  
  /* png structs initialization */
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  png_infop info_ptr = nullptr;
  if(!png_ptr) return GfxError_Unknown;
  
  try
  {
    info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) throw GfxError_Unknown;
    
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
    std::vector<png_unknown_chunk> chunks;
    chunks.reserve(img->userChunks.size());
    for(GfxChunk *chunk : img->userChunks)
    {
      png_unknown_chunk ch;
      memcpy(ch.name, chunk->name, 4);
      ch.name[4] = 0x00;
      ch.data = chunk->data.data();
      ch.size = chunk->data.size();
      ch.location = PNG_AFTER_IDAT;
      
      chunks.push_back(ch);
    }
    // name, data, size, location
    png_set_unknown_chunks(png_ptr, info_ptr, chunks.data(), chunks.size());
    /* keep unsafe-to-copy chunks */
    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS, 0, 0);
    
    
    
    int bit_depth, color_type, bits_per_pixel;
    switch(img->colorFormat)
    {
      default: throw GfxError_ColorFormat;
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
      int nColors = img->palette.size() / 3;
      if(bit_depth == 4 && nColors > 16) nColors = 16;
      png_color palette[nColors];
      for(int i = 0; i < nColors; ++i)
      {
        palette[i].red = img->palette[i * 3];
        palette[i].green = img->palette[i * 3 + 1];
        palette[i].blue = img->palette[i * 3 + 2];
      }
      png_set_PLTE(png_ptr, info_ptr, palette, nColors);
    }
    
    
    
    int bytes_per_row = img->width * ((bits_per_pixel + 7) / 8);
    png_bytep row_pointers[img->height];
    for(int y = 0; y < img->height; ++y)
    {
      row_pointers[y] = new png_byte[bytes_per_row];
      memcpy(row_pointers[y], &(img->pixels[y * bytes_per_row]), bytes_per_row);
    }
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, 0);
    
    for(int y = 0; y < img->height; ++y)
    {
      delete row_pointers[y];
    }
  }
  catch(GfxError &err)
  {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return err;
  }
  
  png_destroy_write_struct(&png_ptr, &info_ptr);
  
  return GfxError_None;
}

static void _writeFn(const void *ptr, int size, void *user)
{
  FILE *fp = (FILE*)user;
  fwrite(ptr, 1, size, fp);
}
int libgfx_writeGfxFile(libgfx_image *img, const char *fname)
{
  FILE *fp = fopen(fname, "wb");
  if(!fp) return GfxError_File;
  
  int ret = libgfx_writeGfx(img, _writeFn, fp);
  fclose(fp);
  
  return ret;
}

int libgfx_loadGfx(libgfx_image *img, GfxReadCb readFn, void *user)
{
  /* user callback initialization */
  _userCb userCb;
  userCb.userData = user;
  userCb.userFn = (void*)readFn;
  
  /* png structs initialization */
  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  png_infop info_ptr = nullptr;
  if(!png_ptr) return GfxError_Unknown;
  
  try
  {
    info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) throw GfxError_Unknown;
    
    
    /* let png know about our user function */
    png_set_read_fn(png_ptr, &userCb, _userReadData);
    
    
    char header[8];
    ((GfxReadCb)userCb.userFn)(header, 5, userCb.userData);
    if(strncmp(header, "AGFX", 4))
    {
      ((GfxReadCb)userCb.userFn)(&header[5], 3, userCb.userData);
      if(png_sig_cmp((png_bytep)header, 0, 8))
      {
        // TODO: is it really AGFX?
        img->fileFormat = GfxFormat_Gfx;
        return GfxError_FileFormat; // not yet supported
      }
    }
    png_set_sig_bytes(png_ptr, 8);
    
    
    png_set_keep_unknown_chunks(png_ptr, 3, 0, -1); // keep all chunks
    
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, 0);
    
    png_unknown_chunkp chunks;
    int nChunks = png_get_unknown_chunks(png_ptr, info_ptr, &chunks);
    
    img->userChunks.reserve(nChunks);
    for(int i = 0; i < nChunks; ++i)
    {
      GfxChunk *userChunk = new GfxChunk;
      memcpy(userChunk->name, chunks[i].name, 4);
      userChunk->name[4] = 0x00;
      userChunk->data.resize(chunks[i].size);
      memcpy(userChunk->data.data(), chunks[i].data, chunks[i].size);
      img->userChunks.push_back(userChunk);
    }
    
    
    img->width = png_get_image_width(png_ptr, info_ptr);
    img->height = png_get_image_height(png_ptr, info_ptr);
    img->pixels.resize(img->width * img->height * 4);
    
    
    
    int color_type = png_get_color_type(png_ptr, info_ptr);
    switch(color_type)
    {
      default: throw GfxError_ColorFormat;
      case PNG_COLOR_TYPE_PALETTE:
      {
        png_colorp palette;
        int nColors;
        png_get_PLTE(png_ptr, info_ptr, &palette, &nColors);
        png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
        GfxRGBA8 *pixels = reinterpret_cast<GfxRGBA8*>(img->pixels.data());
        int bit_depth = img->bitDepth = png_get_bit_depth(png_ptr, info_ptr);
        
        png_bytep trans;
        int num_trans;
        //png_color_16p trans_values; // values
        png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, nullptr);
        
        if(bit_depth == 8)
        {
          for(int i = 0; i < img->width * img->height; ++i)
          {
            int x = i % img->width;
            int y = i / img->width;
            
            int ind = row_pointers[y][x];
            png_color col = palette[ind];
            
            pixels[i].red = col.red;
            pixels[i].green = col.green;
            pixels[i].blue = col.blue;
            //pixels[i].alpha = (ind == 0) ? 0 : 255;
            if(trans && ind < num_trans) pixels[i].alpha = trans[ind];
            else pixels[i].alpha = 255;
          }
        }
        else if(bit_depth == 4)
        {
          for(int i = 0; i < img->width * img->height; i += 2)
          {
            int x = i % img->width;
            int y = i / img->width;
            
            int ind = (row_pointers[y][x / 2] >> 4) & 0xF;
            png_color col = palette[ind];
            
            pixels[i].red = col.red;
            pixels[i].green = col.green;
            pixels[i].blue = col.blue;
            //pixels[i].alpha = (ind == 0) ? 0 : 255;
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
        else throw GfxError_BitDepth;
        break;
      }
      case PNG_COLOR_TYPE_RGB: case PNG_COLOR_TYPE_RGB_ALPHA:
      {
        /*if(color_type == PNG_COLOR_TYPE_RGB_ALPHA) img->colorFormat = GfxFormat_RGBA;
        else img->colorFormat = GfxFormat_RGB;*/
        img->colorFormat = GfxFormat_RGBA;
        
        int bit_depth = img->bitDepth = png_get_bit_depth(png_ptr, info_ptr); // assuming 8
        if(bit_depth != 8) return GfxError_ColorFormat;
        png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
        GfxRGBA8 *pixels = reinterpret_cast<GfxRGBA8*>(img->pixels.data());
        
        for(int i = 0; i < img->width * img->height; ++i)
        {
          int x = i % img->width;
          int y = i / img->width;
          int bpp = (color_type == PNG_COLOR_TYPE_RGB_ALPHA) ? 4 : 3;
          GfxRGBA8 *col = reinterpret_cast<GfxRGBA8*>(&row_pointers[y][x * bpp]);
          
          pixels[i].red = col->red;
          pixels[i].green = col->green;
          pixels[i].blue = col->blue;
          pixels[i].alpha = (bpp == 4) ? col->alpha : 255;
        }
        
        break;
      }
    }
  }
  catch(GfxError &err)
  {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    return err;
  }
  
  png_destroy_read_struct(&png_ptr, &info_ptr, 0);
  
  return 0;
}

static void _readFn(void *ptr, int size, void *user)
{
  FILE *fp = (FILE*)user;
  fread(ptr, 1, size, fp);
}
int libgfx_loadGfxFile(libgfx_image *img, const char *fname)
{
  FILE *fp = fopen(fname, "rb");
  if(!fp) return GfxError_File;
  
  int ret = libgfx_loadGfx(img, _readFn, fp);
  fclose(fp);
  
  return ret;
}
