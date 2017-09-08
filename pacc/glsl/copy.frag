uniform sampler2D palette;
uniform sampler2D tex;
varying mediump vec2 texcoord;
void main(void) {
  lowp float index = texture2D(tex, texcoord).x;
  lowp float color = (index * 255.0 + 0.5) / 256.0;
  gl_FragColor = texture2D(palette, vec2(color, 0.0));
}

