uniform sampler2D palette;
varying mediump vec2 texcoord;
void main(void) {
  gl_FragColor = texture2D(palette, vec2(texcoord.x, 0.0));
}
