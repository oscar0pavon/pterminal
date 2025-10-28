#include "opengl.h"
#include <GL/gl.h>
#include <stdlib.h>
#include <stdio.h>
#include <lodepng.h>
#include <math.h>

bool is_opengl = false;

typedef struct UV{
    float x;
    float y;
}UV;

GLuint font_texture_id;

void gl_draw_rect(PColor color,  float x, float y, float width, float height){
    glColor4f(color.r, color.g, color.b,1.f); 

    glBegin(GL_QUADS);
        glVertex3f(x, y,-1);
        glVertex3f(x + width, y,-1);
        glVertex3f(x + width, y + height,-1);
        glVertex3f(x, y + height,-1);
    glEnd();

}

void gl_draw_char(char character, PColor color,  float x, float y, float width, float height){

    int ascii_value = (int)character;

    int char_x = ascii_value%16;
    int char_y = floor(ascii_value/16);

    float char_size_x = 32.f/512.f;
    float char_size_y = 32.f/512.f;


    UV uv1;
    UV uv2;
    UV uv3;
    UV uv4;

    //button left
    uv1.x = (float)char_x*char_size_x;
    uv1.y = (float)char_y*char_size_y;

    //button right
    uv2.x = (char_x+1)*char_size_x;
    uv2.y = char_y*char_size_y;


    //top right
    uv3.x = (char_x+1)*char_size_x;
    uv3.y = (char_y+1)*char_size_y;


    //top left
    uv4.x = char_x*char_size_x;
    uv4.y = (char_y+1)*char_size_y;
    
    float alpha_value = 0.f;

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_2D, font_texture_id);

    glColor3f(color.r, color.g, color.b); 

    glBegin(GL_QUADS);
    glTexCoord2f(uv1.x, uv1.y);
        glVertex2f(x, y);
    glTexCoord2f(uv2.x, uv2.y);
        glVertex2f(x + width, y);
    glTexCoord2f(uv3.x, uv3.y);
        glVertex2f(x + width, y + height);
    glTexCoord2f(uv4.x, uv4.y);
        glVertex2f(x, y + height);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    
    glDisable(GL_BLEND);


}
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
