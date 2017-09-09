VERT_IN vec4 coord;
VERT_OUT vec2 texcoord;
void main(void) {
  gl_Position = vec4(coord.xy, 0.0, 1.0);
  texcoord = coord.zw;
}

