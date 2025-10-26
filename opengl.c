#include "opengl.h"
#include <stdlib.h>
#include <stdio.h>
#include <lodepng.h>

GLuint font_texture_id;

void set_ortho_projection(float width, float height){


    glLoadIdentity();

    //2D projection where 0,0 is top-left and width,height is bottom-right
    float left = 0.0f;
    float right = (float)width; //window's width
    float bottom = (float)height; //window's height
    float top = 0.0f;
    float near = -1.0f; // Near clipping plane
    float far = 1.0f;   // Far clipping plane

    glOrtho(left, right, bottom, top, near, far);

}


void load_texture(GLuint* texture_pointer, const char* path){

    unsigned int width, height, channels;
    unsigned char* image_data = NULL; 


    unsigned int error = lodepng_decode32_file(&image_data,&width,&height,path);

    

    
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
