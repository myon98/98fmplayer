uniform sampler2D palette;
uniform sampler2D tex;
FRAG_IN mediump vec2 texcoord;
FRAGCOLOR_DECL
void main(void) {
  lowp float index = TEXTURE2D(tex, texcoord).x;
  lowp float color = (index * 255.0 + 0.5) / 256.0;
  FRAGCOLOR = TEXTURE2D(palette, vec2(color, 0.0));
}

