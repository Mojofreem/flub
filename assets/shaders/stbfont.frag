/* OpenGL fragment shader for STB fonts.
   STB fonts only contain alpha color data, so we mix the texture alpha with
   the passed in color value.
*/

uniform sampler2D tex;

void main() {
    vec4 color = texture2D(tex, gl_TexCoord[0].st);
    gl_Color.a = color.a * gl_Color.a;
    gl_FragColor = gl_Color;
}
