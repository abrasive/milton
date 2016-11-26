
#if HAS_MULTISAMPLE
uniform sampler2DMS u_canvas;
#else
uniform sampler2D u_canvas;
#endif
uniform vec2      u_screen_size;
out vec4 out_color;

void main()
{
    vec2 coord = gl_FragCoord.xy / u_screen_size;
#if HAS_MULTISAMPLE
    vec3 color = texelFetch(u_canvas, ivec2(gl_FragCoord.xy), 0).rgb;
#else
    vec3 color = texture(u_canvas, coord).rgb;
#endif


    out_color = vec4(vec3(1)-color, 1);
}
