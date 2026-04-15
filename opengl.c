#include "opengl.h"

#include <GL/gl.h>

#include <pfonts/pfonts.h>
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

      pfonts_load_image_data(image_data, width, height);

      free(image_data);
    }else{
        printf("Can't load image texture\n");
    }

}
