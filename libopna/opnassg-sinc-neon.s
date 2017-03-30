@ neon register map:
@  0,  3,  6,  9, 12, 15 ssg1
@  1,  4,  7, 10, 13, 16 ssg2
@  2,  5,  8, 11, 14, 17 ssg3
@ 18, 19, 20, 21, 22, 23 sinc
@ 24-25 (q12): ssg1 out
@ 26-27 (q13): ssg2 out
@ 28-29 (q14): ssg3 out

.global opna_ssg_sinc_calc_neon
@ r0: resampler_index
@ r1: const int16_t *inbuf
@ r2: int32_t *outbuf

opna_ssg_sinc_calc_neon:
  push {r4-r10,lr}
@ sinc table to r3
  movw r3, #:lower16:opna_ssg_sinctable
  movt r3, #:upper16:opna_ssg_sinctable
  tst r0, #1
  addeq r3, #256

@ add offset to ssg input buffer address
  bic r0, #1
  add r0, r0, lsl #1
  add r1, r0

@ initialize output register
  vmov.i64 q12, #0
  vmov.i64 q13, #0
  vmov.i64 q14, #0

@ sinc sample length
  mov r0, #128

.loop:
@
  subs r0, #24
  blo .end

@ load SSG channel data
  vld3.16 {d0-d2}, [r1]!
  vld3.16 {d3-d5}, [r1]!
  vld3.16 {d6-d8}, [r1]!
  vld3.16 {d9-d11}, [r1]!
  vld3.16 {d12-d14}, [r1]!
  vld3.16 {d15-d17}, [r1]!

@ load sinc data
  vld1.16 {d18-d21}, [r3]!
  vld1.16 {d22-d23}, [r3]!

@ multiply and accumulate
  vmlal.s16 q12, d0,  d18
  vmlal.s16 q13, d1,  d18
  vmlal.s16 q14, d2,  d18
  vmlal.s16 q12, d3,  d19
  vmlal.s16 q13, d4,  d19
  vmlal.s16 q14, d5,  d19
  vmlal.s16 q12, d6,  d20
  vmlal.s16 q13, d7,  d20
  vmlal.s16 q14, d8,  d20
  vmlal.s16 q12, d9,  d21
  vmlal.s16 q13, d10, d21
  vmlal.s16 q14, d11, d21
  vmlal.s16 q12, d12, d22
  vmlal.s16 q13, d13, d22
  vmlal.s16 q14, d14, d22
  vmlal.s16 q12, d15, d23
  vmlal.s16 q13, d16, d23
  vmlal.s16 q14, d17, d23
  b .loop

.end:
@ 8 samples left
  vld3.16 {d0-d2}, [r1]!
  vld3.16 {d3-d5}, [r1]
  vld1.16 {d18-d19}, [r3]

  vmlal.s16 q12, d0, d18
  vmlal.s16 q13, d1, d18
  vmlal.s16 q14, d2, d18
  vmlal.s16 q12, d3, d19
  vmlal.s16 q13, d4, d19
  vmlal.s16 q14, d5, d19

@ extract data from result SIMD registers

  vmov.32 r0,  d24[0]
  vmov.32 r1,  d24[1]
  vmov.32 r3,  d25[0]
  vmov.32 r12, d25[1]

  vmov.32 r14, d26[0]
  vmov.32 r4,  d26[1]
  vmov.32 r5,  d27[0]
  vmov.32 r6,  d27[1]

  vmov.32 r7,  d28[0]
  vmov.32 r8,  d28[1]
  vmov.32 r9,  d29[0]
  vmov.32 r10, d29[1]

  add r0, r1
  add r3, r12

  add r14, r4
  add r5, r6

  add r7, r8
  add r9, r10

  add r4, r0, r3
  add r5, r14
  add r6, r7, r9

  stmia r2, {r4-r6}
  pop {r4-r10,pc}
