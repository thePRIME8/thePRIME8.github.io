//
//  main.cpp
//  gpu_pt
//
//  Created by Trenton Millner on 5/21/15.
//  Copyright (c) 2015 Trenton Millner. All rights reserved.
//


#include <stdio.h>
#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>

SDL_Window* window;
SDL_GLContext gl_context;
SDL_DisplayMode current;
SDL_Event event;

int width = 800;
int height = 600;
unsigned int pt_program;
unsigned int ss_program;

unsigned int ss_vbo;
unsigned int ss_st;
unsigned int ss_vao;

unsigned int pingpong_fbo0;
unsigned int pingpong_fbo1;
unsigned int pt_pingpong0;
unsigned int pt_pingpong1;
unsigned int pt_t_pingpong0;//total accumulated color before gamma correction
unsigned int pt_t_pingpong1;

float cam_x = 0.0;
float cam_y = 0.0;
float cam_z = 3.5;

int framecount = 0;

float ss_quad_pos[] = {
    -1.0, -1.0,
    1.0, -1.0,
    1.0, 1.0,
    1.0, 1.0,
    -1.0, 1.0,
    -1.0, -1.0
};
float ss_quad_st[] = {
    0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    1.0, 1.0,
    0.0, 1.0,
    0.0, 0.0
};

const char* ss_vs =
"#version 400\n"
"layout(location = 0)in vec2 vpos;"
"layout(location = 1)in vec2 st;"
"out vec2 o_st;"
"void main(){"
"   o_st = st;"
"   gl_Position = vec4(vpos, -0.2, 1.0);"
"}";
const char* ss_fs =
"#version 400\n"
"in vec2 o_st;"
"uniform sampler2D input_texture;"
"out vec4 frag_colour;"
"void main () {"
"   frag_colour = texture(input_texture, o_st);"
"}";

const char* pt_vs =
"#version 400 core\n"
"layout(location = 0)in vec2 vpos;"
"layout(location = 1)in vec2 st;"
"smooth out vec2 o_st;"
"void main(){"
"   o_st = st;"
"   gl_Position = vec4(vpos, 0.0, 1.0);"
"}";

const char* pt_fs =
"#version 400 core\n"

"const float INFINITY = 1e10;"

"in vec2 o_st;"

"uniform int width;"
"uniform int height;"
"uniform int frame_count;"
"uniform vec3 cam_pos;"
"uniform sampler2D accumulated;"

"layout(location = 0)out vec4 gc_color;"
"layout(location = 1)out vec4 t_color;"


"struct Sphere{"
"	vec3 center;"
"	float radius;"
"};"
//construct array of spheres
"	Sphere sph[1];"





"float intersect_sphere(vec3 ro, vec3 rd, Sphere sph, float closest_hit){"
"	vec3 rc = ro - sph.center;"
"	float c = dot(rc, rc) - (sph.radius * sph.radius);"
"	float b = dot(rd, rc);"
"	float d = b*b - c;"
"	float t = -b - sqrt(abs(d));"

"	if (d < 0.0 || t < 0.0 || t > closest_hit) {"
"		return INFINITY;"
"	}else {"
"       return t;"
"}"
"}"



"float intersect_spheres(vec3 ro, vec3 rd){"
"	float hit;"
"	float closest;"
"	closest = INFINITY;"

"	for(int i = 0; i < sph.length(); i++){"
"		hit = intersect_sphere(ro, rd, sph[i], INFINITY);"
"		if(hit < closest)"
"			closest = hit;"
"	}"
"	return closest;"
"}"





"float intersect_scene(vec3 ro, vec3 rd){"
"	float sph = intersect_spheres(ro, normalize(rd));"//closest sphere intersection

"	return sph;"

"}"



"void build_spheres(){"
"	sph[0].center = vec3(0.0, 0.0, 0.0);"
"	sph[0].radius = 1.0;"

"}"




"void main(){"

"	build_spheres();"

"	vec2 resolution = vec2(width, height);"
"	vec3 col = vec3(0.0);"

"	vec2 p = (-resolution + 2.0*(gl_FragCoord.xy )) / resolution.y;"

//camera variables and ray origin
"	vec3 ro = cam_pos;"

"   vec3 rd = normalize(vec3(p, -1.0));"


"	float hit = intersect_scene(ro, normalize(rd));"//intersect scene

"   if(hit < INFINITY)"
"       col = vec3(1.0);"
"   else"
"       col = vec3(0.0);"

"	t_color = vec4(col, 1.0);"
"	gc_color = vec4(col, 1.0);"

"}";




unsigned int compile_shader(const char* v_source, const char* f_source){
    int vs_status = -1;
    int fs_status = -1;
    
    unsigned int vs = glCreateShader (GL_VERTEX_SHADER);
    glShaderSource (vs, 1, &v_source, NULL);
    glCompileShader (vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &vs_status);
    if(vs_status != GL_TRUE){
        int length;
        char buffer[1000];
        glGetShaderInfoLog(vs, sizeof(buffer), &length, buffer);
        printf("Vertex Shader ID:%i OpenGL Shader Compile Error at %s", vs, buffer);
        printf("Vertex Shader ID:%i OpenGL Shader Compile Error at %s", vs, buffer);
    }
    unsigned int fs = glCreateShader (GL_FRAGMENT_SHADER);
    glShaderSource (fs, 1, &f_source, NULL);
    glCompileShader (fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &fs_status);
    if(fs_status != GL_TRUE){
        int length;
        char buffer[1000];
        glGetShaderInfoLog(fs, sizeof(buffer), &length, buffer);
        printf("FragmentShader ID:%i OpenGL Shader Compile Error at %s\n", fs, buffer);
        printf("Fragmemt Shader ID:%i OpenGL Shader Compile Error at %s\n", fs, buffer);
        
    }
    unsigned int program = glCreateProgram ();
    glAttachShader (program, fs);
    glAttachShader (program, vs);
    glLinkProgram (program);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    return program;
}

void reset(){
    //init pingpong tex 2 with some content
    float* data = (float*)malloc(height * width * 4 * sizeof(float));
    GLubyte val = 0;
    for(int i = 0; i < height * width * 4; i++){
        data[i] = val;
    }
    glBindTexture(GL_TEXTURE_2D, pt_pingpong0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, &data[0]);
    glBindTexture(GL_TEXTURE_2D, pt_pingpong1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, &data[0]);
    glBindTexture(GL_TEXTURE_2D, pt_t_pingpong0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, &data[0]);
    glBindTexture(GL_TEXTURE_2D, pt_t_pingpong1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, &data[0]);
    
    framecount = 0;
}

void init(){
    //init SDL
    if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
        printf("SDL failed to initialized\n");
    printf("SDL initialized\n");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    window = SDL_CreateWindow("GPU Pathtracer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    SDL_SetWindowPosition(window, 50, 50);
    
    gl_context = SDL_GL_CreateContext(window);
    if (gl_context == NULL){
        printf("There was an error creating the OpenGL context.");
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GetWindowDisplayMode(window, &current);
    printf("OpenGL VERSION: %s\n",glGetString(GL_VERSION));
    printf("OpenGL RENDERER: %s\n",glGetString(GL_RENDERER));
    printf("GLSL VERSION: %s\n",glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("OpenGL VENDOR: %s\n\n",glGetString(GL_VENDOR));
    
    
    
    //init
    pt_program = compile_shader(pt_vs, pt_fs);
    ss_program = compile_shader(ss_vs, ss_fs);
    
    //attach to fbo
    glGenFramebuffers(1, &pingpong_fbo0);
    glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo0);
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &pt_pingpong0);
    glBindTexture(GL_TEXTURE_2D, pt_pingpong0);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pt_pingpong0, 0 );
    
    glGenTextures(1, &pt_t_pingpong0);
    glBindTexture(GL_TEXTURE_2D, pt_t_pingpong0);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, pt_t_pingpong0, 0 );
    
    GLenum pingpong0_draw_buffs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, pingpong0_draw_buffs);
    //check fbo status
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    //printf("Pingpong frameBuffer status: 0x%x\n", status);
    printf("Pingpong frameBuffer 0 status: 0x%x, 0x8cd5 is complete\n", status);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    glGenFramebuffers(1, &pingpong_fbo1);
    glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo1);
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &pt_pingpong1);
    glBindTexture(GL_TEXTURE_2D, pt_pingpong1);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    //init pingpong tex 2 with some content
    float* data = (float*)malloc(height * width * 4 * sizeof(float));
    GLubyte val = 0;
    for(int i = 0; i < height * width * 4; i++){
        data[i] = val;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, &data[0]);
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pt_pingpong1, 0 );
    
    
    glGenTextures(1, &pt_t_pingpong1);
    glBindTexture(GL_TEXTURE_2D, pt_t_pingpong1);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    //init pingpong tex 2 with some content
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, &data[0]);
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, pt_t_pingpong1, 0 );
    GLenum pingpong1_draw_buffs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, pingpong1_draw_buffs);
    //check fbo status
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    printf("Pingpong frameBuffer 1 status: 0x%x, 0x8cd5 is complete\n", status);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    
    //init screenspace quad
    glGenBuffers(1, &ss_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ss_vbo);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), &ss_quad_pos, GL_STATIC_DRAW);
    glGenBuffers(1, &ss_st);
    glBindBuffer(GL_ARRAY_BUFFER, ss_st);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), &ss_quad_st, GL_STATIC_DRAW);
    glGenVertexArrays(1, &ss_vao);
    glBindVertexArray(ss_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ss_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, ss_st);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(1);
}

void draw_to_screen(unsigned int vao, unsigned int texture){
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texture);
    glUseProgram(ss_program);
    glUniform1i(glGetUniformLocation(ss_program,"input_texture"),0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void pt(){
    glUseProgram(pt_program);
    glBindVertexArray(ss_vao);
    
    if(framecount%2 == 0){
        glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo0);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, width, height);
        
        //glm::mat4 cam_mat = glm::lookAt(glm::vec3(cam_x,cam_y,cam_z), glm::vec3(tar_x,tar_y,tar_z), glm::vec3(up_x,up_y,up_z));//
        float cam_pos[3] = {cam_x, cam_y, cam_z};
        glUniform3fv(glGetUniformLocation(pt_program,"cam_pos"),1, cam_pos);
        glUniform1i(glGetUniformLocation(pt_program, "width"), width);
        glUniform1i(glGetUniformLocation(pt_program, "height"), height);
        glUniform1i(glGetUniformLocation(pt_program, "frame_count"), framecount);
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, pt_t_pingpong1);
        glUniform1i(glGetUniformLocation(pt_program,"accumulated"),0);
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        draw_to_screen(ss_vao, pt_pingpong0);
        SDL_GL_SwapWindow(window);
        
    }else{
        glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo1);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, width, height);
        
        
        //glm::mat4 cam_mat = glm::lookAt(glm::vec3(cam_x,cam_y,cam_z), glm::vec3(tar_x,tar_y,tar_z), glm::vec3(up_x,up_y,up_z));//
        float cam_pos[3] = {cam_x, cam_y, cam_z};
        glUniform3fv(glGetUniformLocation(pt_program,"cam_pos"),1, cam_pos);
        glUniform1i(glGetUniformLocation(pt_program, "width"), width);
        glUniform1i(glGetUniformLocation(pt_program, "height"), height);
        glUniform1i(glGetUniformLocation(pt_program, "frame_count"), framecount);
        //glUniformMatrix4fv(glGetUniformLocation(tracer_program,"mvp"), 1, GL_FALSE, &mvp[0][0]);
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, pt_t_pingpong0);
        glUniform1i(glGetUniformLocation(pt_program,"accumulated"),0);
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        draw_to_screen(ss_vao, pt_pingpong1);
        SDL_GL_SwapWindow(window);
    }
    framecount++;
}


int input(){
    
    int exit = 1;
    while(SDL_PollEvent(&event)){
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE){
            exit = 0;
        }
    }
    return exit;
    
}


int main() {
    framecount = 0;
    init();
    while(input()){
        pt();
    }
    
    return 0;
}
