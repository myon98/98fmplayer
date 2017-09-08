struct psinput {
  float4 texcoord: TEXCOORD;
};

struct psoutput {
  float4 fragcolor: COLOR;
};

sampler palette: register(s0);

psoutput fill(psinput input) {
  psoutput output;
  output.fragcolor = tex1D(palette, input.texcoord.x);
  return output;
}
