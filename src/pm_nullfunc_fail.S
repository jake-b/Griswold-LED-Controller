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
 *
 * system_restart() is not a restart. It stops the station, arms a 100ms
 * timer, and returns. This runs on the Wi-Fi task, so that timer never
 * arrives. Looping back into system_restart starved the scheduler, and the
 * software watchdog fired at the ret.n in ets_intr_unlock (epc1 0x4010075f).
 * Boot then kept the watchdog record, so the marker was ignored.
 *
 * Call the unwrapped system_restart_local(), which resets from this task
 * and stores REASON_SOFT_RESTART. It leaves a stored reason of 2 or 3
 * alone, and the RTC still holds the watchdog record, so clear that word
 * first. Feed the watchdog before the reset so a pending stage cannot win.
 */
.section .irom0.text, "ax"
.align 4
.literal_position
.literal .Lrtc, 0x600014e0
.literal .Lrtcwrite, system_rtc_mem_write
.literal .Lfeed, pp_soft_wdt_feed
.literal .Lrestart, __real_system_restart_local
.align 4
.global griswold_nullfunc_fail
.type griswold_nullfunc_fail, @function
griswold_nullfunc_fail:
  l32r a2, .Lrtc
  movi a3, 0x6e
  slli a3, a3, 8
  addi a3, a3, 0x31
  s32i a3, a2, 0
  memw
  addi a1, a1, -16
  movi a2, 0
  s32i a2, a1, 0
  mov.n a3, a1
  movi a4, 4
  l32r a0, .Lrtcwrite
  callx0 a0
  l32r a0, .Lfeed
  callx0 a0
  l32r a0, .Lrestart
  callx0 a0
.Lhang:
  j .Lhang
.size griswold_nullfunc_fail, . - griswold_nullfunc_fail
