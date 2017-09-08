uniform sampler2D palette;
uniform sampler2D tex;
varying mediump vec2 texcoord;
uniform lowp float key;
uniform lowp float color;
void main(void) {
  lowp float index = texture2D(tex, texcoord).x;
  if (index < (key + (0.5/255.0)) || (key + (1.5/255.0)) < index) {
    discard;
  }
  gl_FragColor = texture2D(palette, vec2(color, 0.0));
}
