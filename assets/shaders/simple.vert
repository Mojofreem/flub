/* A simple vertex shader. This shader adjusts for the projection/view matrix only. */

void main() {
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_FrontColor = gl_Color;
    gl_Position = ftransform();
}
