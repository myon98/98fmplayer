struct vsinput {
  float4 coord : POSITION;
};

struct fsinput {
  float4 coord : POSITION;
  float4 texcoord : TEXCOORD;
};

float2 offset: register(c0);

fsinput blit(vsinput input) {
  fsinput output;
  output.coord = float4(input.coord.xy + offset, 0.0, 1.0);
  output.texcoord = float4(input.coord.zw, 0.0, 1.0);
  return output;
}
