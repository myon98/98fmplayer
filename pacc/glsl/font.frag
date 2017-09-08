uniform sampler2D palette;
uniform sampler2D tex;
varying mediump vec2 texcoord;
uniform lowp float bg;
uniform lowp float color;
void main(void) {
  gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
  lowp float pixel = texture2D(tex, texcoord).x;
  lowp float index = color;
  if (pixel < 0.5) {
    if (bg < 0.5) discard;
    index = 0.5 / 256.0;
  }
  gl_FragColor = texture2D(palette, vec2(index, 0.0));
}
