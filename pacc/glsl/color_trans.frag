uniform sampler2D palette;
uniform sampler2D tex;
varying mediump vec2 texcoord;
uniform lowp float color;
void main(void) {
  lowp float index = texture2D(tex, texcoord).x;
  if (index < (0.5/255.0)) {
    discard;
  }
  gl_FragColor = texture2D(palette, vec2(color, 0.0));
}
