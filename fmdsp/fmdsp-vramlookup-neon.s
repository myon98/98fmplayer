@ neon register map
@  0,  3,  6,  9, 12, 15, 18, 21  b
@  1,  4,  7, 10, 13, 16, 19, 22  g
@  2,  5,  9, 11, 14, 17, 20, 23  r

@ 16, 17, 18, 19, 20, 21, 22, 23  vram

@ 26, 27 r palette
@ 28, 29 g palette
@ 30, 31 b palette

.global fmdsp_vramlookup_neon

@ r0: uint8_t *vram32
@   4 bytes aligned
@   b, g, r, 0,
@ r1: const uint8_t *vram
@ r2: const uint8_t *palette
@   r0, g0, b0, r1, g1, b1, ...
@ r3: int stride
fmdsp_vramlookup_neon:
  push {lr}
@ load palette
  vld3.8 {d26, d28, d30}, [r2]!
  vld3.8 {d27[0], d29[0], d31[0]}, [r2]!
  vld3.8 {d27[1], d29[1], d31[1]}, [r2]!

  mov r14, #400
.loopcol:
  mov r2, r0
  mov r12, #10
.looprow:
@ row address

@ load vram
  vld1.8 {d16-d19}, [r1]!
  vld1.8 {d20-d23}, [r1]!

@ lookup
  vtbl.8 d0,  {d30-d31}, d16
  vtbl.8 d1,  {d28-d29}, d16
  vtbl.8 d2,  {d26-d27}, d16
  vtbl.8 d3,  {d30-d31}, d17
  vtbl.8 d4,  {d28-d29}, d17
  vtbl.8 d5,  {d26-d27}, d17
  vtbl.8 d6,  {d30-d31}, d18
  vtbl.8 d7,  {d28-d29}, d18
  vtbl.8 d8,  {d26-d27}, d18
  vtbl.8 d9,  {d30-d31}, d19
  vtbl.8 d10, {d28-d29}, d19
  vtbl.8 d11, {d26-d27}, d19
  vtbl.8 d12, {d30-d31}, d20
  vtbl.8 d13, {d28-d29}, d20
  vtbl.8 d14, {d26-d27}, d20
  vtbl.8 d15, {d30-d31}, d21
  vtbl.8 d16, {d28-d29}, d21
  vtbl.8 d17, {d26-d27}, d21
  vtbl.8 d18, {d30-d31}, d22
  vtbl.8 d19, {d28-d29}, d22
  vtbl.8 d20, {d26-d27}, d22
  vtbl.8 d21, {d30-d31}, d23
  vtbl.8 d22, {d28-d29}, d23
  vtbl.8 d23, {d26-d27}, d23

@ store vram32
  vst4.8 {d0-d3},   [r2]!
  vst4.8 {d3-d6},   [r2]!
  vst4.8 {d6-d9},   [r2]!
  vst4.8 {d9-d12},  [r2]!
  vst4.8 {d12-d15}, [r2]!
  vst4.8 {d15-d18}, [r2]!
  vst4.8 {d18-d21}, [r2]!
  vst4.8 {d21-d24}, [r2]!

  subs r12, #1
  bne .looprow

  add r0, r3
  subs r14, #1
  bne .loopcol

  pop {pc}
