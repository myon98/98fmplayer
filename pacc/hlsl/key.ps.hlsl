struct psinput {
  float4 texcoord: TEXCOORD;
};

struct psoutput {
  float4 fragcolor: COLOR;
};

sampler palette: register(s0);
sampler tex: register(s1);
float uni_key: register(c0);
float color: register(c1);

psoutput key(psinput input) {
  psoutput output;
  float index = tex2D(tex, input.texcoord.xy).r;
  if (index < (uni_key + (0.5/255.0)) || (uni_key + (1.5/255.0)) < index) discard;
  output.fragcolor = tex1D(palette, color);
  return output;
}
