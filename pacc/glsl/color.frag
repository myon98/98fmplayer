uniform sampler2D palette;
uniform sampler2D tex;
varying mediump vec2 texcoord;
uniform lowp float color;
void main(void) {
  lowp float index = texture2D(tex, texcoord).x;
  if (index > (0.5/255.0)) {
    index = color;
  } else {
    index = 0.5 / 256.0;
  }
  gl_FragColor = texture2D(palette, vec2(index, 0.0));
}
