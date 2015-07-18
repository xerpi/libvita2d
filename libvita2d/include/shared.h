#ifndef SHARED_H
#define SHARED_H

/* Shared with other .c */
extern float ortho_matrix[4*4];
extern SceGxmContext *context;
extern SceGxmVertexProgram *colorVertexProgram;
extern SceGxmFragmentProgram *colorFragmentProgram;
extern SceGxmVertexProgram *textureVertexProgram;
extern SceGxmFragmentProgram *textureFragmentProgram;
extern const SceGxmProgramParameter *colorWvpParam;
extern const SceGxmProgramParameter *textureWvpParam;


#endif
