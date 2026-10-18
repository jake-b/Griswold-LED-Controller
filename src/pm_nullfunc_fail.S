/* pm_send_nullfunc calls this instead of spinning when the management
 * buffer pool is empty. Two callers inside pm_suspend then set the power
 * state to "null frame sent". Nothing was sent, the radio waits for a TX
 * completion that never comes, and loop() keeps blinking until a power cycle.
 * Those two returns are the only ones whose bytes at +0x11 are 09 c2, which
 * is the store, and +0x1f is the caller's epilogue. Skip the store there.
 * Every other caller returns normally. A send that got a buffer never gets
 * here. a1 is still pm_send_nullfunc's frame.
 */
.section .irom0.text, "ax"
.align 4
.begin no-transform
.global griswold_nullfunc_fail
.type griswold_nullfunc_fail, @function
griswold_nullfunc_fail:
  l32i.n a2, a1, 32
  l8ui   a3, a2, 0x11
  addi   a3, a3, -9
  bnez.n a3, .Ldone
  l8ui   a3, a2, 0x12
  movi   a4, 0xc2
  bne    a3, a4, .Ldone
  addi   a2, a2, 0x1f
  s32i.n a2, a1, 32
.Ldone:
  l32i.n a12, a1, 36
  l32i.n a13, a1, 40
  l32i.n a0, a1, 32
  addi   a1, a1, 48
  ret.n
.size griswold_nullfunc_fail, . - griswold_nullfunc_fail
.end no-transform
