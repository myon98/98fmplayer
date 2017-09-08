struct psinput {
  float4 texcoord: TEXCOORD;
};

struct psoutput {
  float4 fragcolor: COLOR;
};

sampler palette: register(s0);
sampler tex: register(s1);
float bg: register(c0);
float color: register(c1);

psoutput font(psinput input) {
  psoutput output;
  float pixel = tex2D(tex, input.texcoord.xy).r;
  float index = color;
  if (pixel < 0.5) {
    if (bg < 0.5) discard;
    index = 0.5 / 256.0;
  }
  output.fragcolor = tex1D(palette, index);
  return output;
}
