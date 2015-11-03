/* Simple OpenGL fragment shader */

uniform sampler2D tex;

void main() {
    vec4 color;

    color = texture2D(tex, gl_TexCoord[0].st);
    color.a = color.a * gl_Color.a;
    gl_FragColor = vec4(color * gl_Color);

    //gl_Color.a = color.a * gl_Color.a;
    //gl_FragColor = gl_Color;

    /*
    float avg = (gl_Color[0] + gl_Color[1] + gl_Color[2]) / 3.0;
    gl_FragColor = vec4(avg, avg, avg, gl_Color.a);
*/
    /* gl_FragColor = vec4(0.4, 0.4, 0.4, gl_Color.a); */
}
