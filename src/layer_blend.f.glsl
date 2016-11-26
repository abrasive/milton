
#if HAS_MULTISAMPLE
uniform sampler2DMS u_canvas;
#else
uniform sampler2D u_canvas;
#endif
uniform vec2      u_screen_size;

out vec4 out_color;

void main()
{
    vec4 g_eraser_magic = vec4(0,1,0,1);


    //vec2 coord = gl_FragCoord.xy/ u_screen_size;
    vec2 coord = vec2(gl_FragCoord.x, u_screen_size.y-gl_FragCoord.y) / u_screen_size;

#if HAS_MULTISAMPLE
    vec4 color = texelFetch(u_canvas, ivec2(gl_FragCoord.xy), 0);
#else
    vec4 color = texture(u_canvas, coord);
#endif

    if (color == g_eraser_magic)
    {
        discard;
    }
    out_color = vec4(color);
}
