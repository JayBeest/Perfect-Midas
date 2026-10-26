#ifndef PORT_AUDIO_PACE_DEFAULTS_H
#define PORT_AUDIO_PACE_DEFAULTS_H

/*
 * The audio pump's pacing configuration: the compiled-in defaults.
 *
 * These live in a header rather than inside port/src/audio_pc.c for one reason:
 * port/tests/audio_pace_env.c has to build a PortAudioPaceCfg from the SAME
 * numbers the pump uses. A test that hard-coded its own copy would keep passing
 * after someone changed a default here, which makes "an unset variable changes
 * nothing" a statement about the test's copy rather than about the game.
 *
 * The values are unchanged from where they were lifted; see the comments, which
 * came with them.
 *
 * WHICH OF THESE CAN BE OVERRIDDEN AT RUN TIME
 *
 * Eight, via GE_AUDIO_* (port/include/audio_pace_env.h): TARGET_MS, TARGET_MAX,
 * TARGET_STEP, TARGET_EASE, CATCHUP_MS, DRAIN_MS, QUEUE_MAX, RATE.
 *
 * The rest are structural and deliberately not tunable:
 *   frame_align        the synthesizer's own rounding contract
 *   max_frame          the bound on one render call
 *   device_period_ms   a property of the device, not of taste
 *   max_interval_ms    a latency policy bound
 * Changing any of those changes what the policy MEANS rather than how it is
 * tuned, so they stay compile-time.
 */

#define PORT_AUDIO_RATE      22050  /* both GE banks; alSynConfig outputRate */
#define PORT_AUDIO_QUEUE_MAX (16 * 1024)
/* Queue depth held in steady state: the latency/safety trade, nothing else.
 * 40 ms is ~3.5 device periods of cushion - enough that a hitchy Windows
 * shared-mode mixer cannot starve it, small enough to stay inaudible on a
 * reload. Tighten only with a device that has proven it never underruns. */
#define PORT_AUDIO_TARGET_MS 40
#define PORT_AUDIO_TARGET    (PORT_AUDIO_RATE * 4 / 1000 * PORT_AUDIO_TARGET_MS)
#define PORT_AUDIO_TARGET_MAX (10 * 1024) /* raised this far by queues that ran dry */
#define PORT_AUDIO_TARGET_STEP 1024       /* per underrun, within the ceiling above */
#define PORT_AUDIO_TARGET_EASE 256        /* per quiet 10 s window, back toward TARGET */
#define PORT_AUDIO_CATCHUP_MS 250         /* wall time one pump may credit */
#define PORT_AUDIO_DRAIN_MS  2000         /* no consumption this long = stalled */
#define PORT_AUDIO_DEV_PERIOD_MS 12       /* cushion over one pump interval */
/* Longest pump interval the queue is asked to bridge. Single-digit frame rates
 * are real (classic and demo modes), so this covers down to 1 fps. Beyond that
 * the gap is a hitch, not a frame rate: audio cannot be synthesised for time
 * that has not been pumped, and a 1 s backlog is already the point where
 * latency stops being worth buying.
 *
 * THE COST, stated plainly: at N fps the pump runs N times a second, so each
 * pump must bridge 1000/N ms of audio, and that backlog IS the audio latency.
 * At 60 fps it is 17 ms and inaudible; at 2 fps it is 500 ms and the music runs
 * half a second behind. That is inherent to pumping from the render loop - the
 * only way out is a separate audio thread, which the AL library's shared state
 * makes a much larger change. Continuous audio at high latency beats crackle at
 * low latency, so that is the trade taken here. */
#define PORT_AUDIO_MAX_INTERVAL_MS 1000
#define PORT_AUDIO_ALIGN     16           /* alAudioFrame rounds to this */
#define PORT_MAX_FRAME_SAMPLES 1024

#endif /* PORT_AUDIO_PACE_DEFAULTS_H */
