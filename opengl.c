#include "opengl.h"

#include <GL/gl.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#if __has_include("./lib/lodepng.h")
#include "./lib/lodepng.h"
#else
#error "lodepng.h missing run: ./configure"
#endif


#include "font.h"



void load_font_image(GLuint* texture_pointer){

    unsigned int width, height, channels;
    unsigned char* image_data = NULL; 


    const unsigned char* font_bytes = _binary_font_png_start;

    size_t size = _binary_font_png_end - _binary_font_png_start;

    unsigned int error = lodepng_decode32(&image_data,&width,&height,
        font_bytes,
        size);

    
    if(image_data && !error){

      glGenTextures(1, texture_pointer);

      glBindTexture(GL_TEXTURE_2D, *texture_pointer);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, image_data);


      free(image_data);
    }else{
        printf("Can't load image texture\n");
    }

}
