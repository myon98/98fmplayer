struct psinput {
  float4 texcoord: TEXCOORD;
};

struct psoutput {
  float4 fragcolor: COLOR;
};

sampler palette: register(s0);
sampler tex: register(s1);
float uni_color: register(c0);

psoutput color_trans(psinput input) {
  psoutput output;
  float index = tex2D(tex, input.texcoord.xy).r;
  if (index < (0.5/255.0)) discard;
  output.fragcolor = tex1D(palette, uni_color);
  return output;
}
