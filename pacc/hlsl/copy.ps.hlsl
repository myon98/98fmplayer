struct psinput {
  float4 texcoord: TEXCOORD;
};

struct psoutput {
  float4 fragcolor: COLOR;
};

sampler palette: register(s0);
sampler tex: register(s1);

psoutput copy(psinput input) {
  psoutput output;
  float index = tex2D(tex, input.texcoord.xy).r;
  float color = (index * 255.0 + 0.5) / 256.0;
  output.fragcolor = tex1D(palette, color);
  return output;
}
