/* pm_send_nullfunc calls this instead of spinning when the management
 * buffer pool is empty. Returning to the caller is not safe. Two sites in
 * pm_suspend then mark the null frame sent, and the radio waits for a TX
 * completion that never comes. Skipping that store is not safe either: the
 * state machine goes on into light sleep, stops the watchdog, and waits for
 * a wakeup that the missing frame will never produce. The CPU stays there.
 * The last WS2811 frame stays latched, so the lights are on and frozen, and
 * the station is gone until a power cycle.
 *
 * Write a marker where boot can see it and restart. RTC user word 120 is
 * absolute word 184 at 0x600014e0, past the 32-word eboot command. The magic
 * is 0x6e31. setup() turns that into "Null frame restart" and clears it.
 * A send that got a buffer never gets here.
 */
.section .irom0.text, "ax"
.align 4
.literal_position
.literal .Lrtc, 0x600014e0
.literal .Lrestart, system_restart
.align 4
.global griswold_nullfunc_fail
.type griswold_nullfunc_fail, @function
griswold_nullfunc_fail:
  l32r a2, .Lrtc
  movi a3, 0x6e
  slli a3, a3, 8
  addi a3, a3, 0x31
  s32i a3, a2, 0
  l32r a0, .Lrestart
  callx0 a0
  j griswold_nullfunc_fail
.size griswold_nullfunc_fail, . - griswold_nullfunc_fail
