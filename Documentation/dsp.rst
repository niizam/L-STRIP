==============
DSP reference
==============

All stages are zero-latency, allocation-free and denormal-safe (the audio
callback sets flush-to-zero).  The tests run at 48 kHz; every time constant
is derived from the sample rate.  Unless stated, processing is stereo-linked:
detectors look at the louder of the two channels.

Input protection
================

Before anything else every input sample is checked: NaN and infinity become
zero and values are limited to +/- 16 (+24 dBFS).  Hosts occasionally hand
over uninitialised memory (OBS does for a missing channel); one NaN would
otherwise poison every stereo-linked detector and the reverb and delay lines
until the plugin is reset.  The output is checked once more before it is
returned to the host.

Stages, in signal order
=======================

Gate
----
Peak detector (0.5 ms attack, 20 ms release) with 4 dB hysteresis, 60 ms
hold, 0.7 ms open and 100 ms close smoothing, and a -70 dB closed floor.
The threshold range is -80 to -20 dB; the bottom of the range means Off.
The gate is zero latency, so the very first milliseconds of a word are
shaped by the 0.7 ms open time rather than looked ahead.

Plosive tamer
-------------
Softens the low-frequency thump of "p" and "b".  A pop is a burst of energy
below about 100 Hz with little above it, while a sung or spoken low note has
strong upper harmonics too.  Two peak followers compare the band below 90 Hz
(1 ms attack, 25 ms release) with the body of the voice above 300 Hz (4 ms
attack, 60 ms release).  Only when the low band clearly dominates, and is
above -48 dBFS, is the signal blended towards a 4th-order 150 Hz high-pass:

    out = x + act * (highpass(x) - x)

``act`` rises in 1.5 ms and falls in 25 ms, so the cut lasts as long as the
pop does and leaves the following word alone.  The amount sets both how
readily it reacts (ratio threshold 5 down to 2.5) and how deep the blend
goes (50 % up to 100 %).  Sung notes measure about 1-2 on that ratio, which
the highest amount stays clear of.  A very dark voice (a fundamental with
almost no upper harmonics) can look like a pop to this detector; lower the
amount then.  At 0 % the module does not run and the signal is bit-exact.
Zero latency, placed before Auto-tune so that thumps do not disturb the
pitch detector.

Auto-tune (pitch correction)
----------------------------
Detector
    A causal YIN detector on a copy of the input decimated to about 12 kHz
    (4th-order Butterworth first).  Window 200 samples, a new estimate every
    32 decimated samples (about 2.7 ms), range 70-900 Hz, absolute threshold
    0.15 with parabolic interpolation, a -56 dBFS energy gate, and a 3-point
    median on log-pitch to remove isolated octave errors.  It only uses past
    samples, so it adds no algorithmic latency; the estimate describes the
    voice a few milliseconds ago.

Control
    The nearest *allowed* note is chosen with 25 cents of hysteresis towards
    the current note.  The offset in cents is split into a slow part (0.2 s
    low-pass) and a fast part; Humanize keeps that fraction of the fast
    part, so vibrato survives while the average is pulled to the note.
    Speed (0-200 ms) is the time constant of the correction; 0 ms gives the
    hard "robotic" effect.  Corrections are limited to +/- 400 cents.  The
    module fades out after about 27 ms of unvoiced signal and holds its
    state through shorter drop-outs.  Correction (0-100 %) scales the
    correction; at 0 % the pitch is left alone.

Transpose
    A constant shift of -12 to +12 semitones added to the correction, so
    the shifter's ratio is ``2^((correction + shift) / 12)``.  With
    Correction at 0 % this is a plain pitch shifter.  The shifter moves the
    formants along with the pitch, so up sounds like a smaller voice and
    down like a larger one; there is no separate formant control.  While
    transposing, breaths and consonants are shifted too (the shifter stays
    on through unvoiced stretches and keeps the last pitch period), so they
    do not jump back to the original pitch.  The latency does not change
    with the shift, because the mean tap delay does not depend on the ratio.

Shifter
    A two-tap delay-line shifter.  The cycle length is two pitch periods, so
    the two taps are one period apart and always phase-coherent, and the
    cross-fade jump is a whole number of periods.  Typical corrections are a
    few cents to a semitone, so the taps sweep slowly and jumps are rare
    (every few hundred milliseconds), which keeps the artefacts of classic
    time-domain shifting low.

Latency
    Fixed 10 ms (480 samples at 48 kHz) while Tune is on.  The minimum tap
    delay is ``10 ms - one period`` so the shifted path averages exactly
    10 ms and the un-shifted path (which is delayed by the same 10 ms)
    stays time-aligned.  This holds for voices of about 105 Hz and up; for
    lower voices the shifted path is later by ``period - 9.5 ms``, which
    can colour the sound slightly when Tune Mix is below 100%.

Auto level
----------
A slow "vocal rider" in front of the compressor.  It measures the loudness
of the voice over about 400 ms and moves a gain towards the amount that
would bring it to -20 dBFS RMS, limited to +/- 12 dB at 100 % (less at
lower amounts).  The gain moves slowly (about 1 s to lift, 0.25 s to ease),
so it works on phrases, not on syllables.

A fast detector (30 ms) decides whether the voice is present, with a floor
of -50 dBFS RMS.  While it is not, both the loudness measurement and the
gain are frozen.  That is what stops it from pumping room noise up in a
pause, and from reading the dying tail of a phrase as a quiet passage.  When
the voice first arrives (after silence, a reset, or raising the amount from
0 %) the gain stays at unity for the first 100 ms of continuous speech and
the loudness measurement starts from the average of that window; otherwise
the gain would swell for a moment before settling.  Switching the amount to
0 % glides the gain back to unity (time constant 0.1 s); with the amount at 0
and the gain at unity the module is bypassed and bit-exact.  Zero latency.

Compressor
----------
Feed-forward, soft knee, in the dB domain.  The detector is RMS-like for
*Clean* and peak-leaning for *Punch* and *Thick*.

============ ======= ========== ==========================================
Style        Ratio   Knee       Notes
============ ======= ========== ==========================================
Clean        2.5:1   8 dB       RMS detector
Punch        4:1     3 dB       attack halved
Thick        8:1     6 dB       peak detector
============ ======= ========== ==========================================

============ =========== ===========================================
Mode         Attack      Release
============ =========== ===========================================
Fast         3 ms        80 ms
Slow         25 ms       300 ms
Auto         8 ms        500 ms down to 50 ms as gain reduction rises
============ =========== ===========================================

Tone is a tilt: a low shelf (400 Hz) and a high shelf (3 kHz) of opposite
sign, each half the Tone value.  Mix is parallel compression against the
dry signal.  Makeup is applied to the compressed path only.

De-esser
--------
A split-band dynamic cut.  Sibilance is the level of a band-pass (Q 1.4,
centre = Freq) *relative to* the vocal body (low-passed at 2.5 kHz), which
works at any level.  The threshold moves from +2 dB (Amount 0) to -14 dB
(Amount 100); the reduction is ``0.8 x (excess)`` limited to Range, with
0.5 ms attack and 40 ms release.  Amount 0 bypasses the stage.  LISTEN
outputs only the band that is being removed.

Equalizer
---------
Five sections: a 12 dB/octave low-cut (off at 20-21 Hz), a low shelf, a mid
bell, a high bell and an air shelf (RBJ biquads, double precision),
followed by an optional high-cut: a 4th-order (24 dB/octave) Butterworth
low-pass from 2 to 20 kHz, off at the top of the range.  The style chooses
the voicing of the four middle sections:

========= ========= ============== ============== ==========
Style     Low shelf Mid bell       High bell      Air shelf
========= ========= ============== ============== ==========
Analog    100 Hz    400 Hz, Q 0.7  3 kHz, Q 0.7   10 kHz
Modern    80 Hz     600 Hz, Q 1.0  4 kHz, Q 1.0   12 kHz
Broadcast 120 Hz    350 Hz, Q 1.2  5 kHz, Q 0.9   14 kHz
========= ========= ============== ============== ==========

The editor draws the curve by evaluating the same biquads (high-cut
included), so the picture is the filter.

Multiband
---------
Linkwitz-Riley 4th-order crossovers at 150 Hz and 3 kHz (minimum phase, the
low band carries an all-pass so the three bands sum flat).  Each band has a
downward and an upward compressor in the dB domain, linked across channels.
Upward boost fades out below -75 dB so silence is never lifted, and is
limited to 18 dB.

============ ============ ====== ============= ======
Style        Down thresh  Ratio  Up thresh     Ratio
============ ============ ====== ============= ======
Smooth       -14 dB       3:1    -36 dB        2:1
Punch        -18 dB       4:1    -32 dB        3:1
OTT          -24 dB       8:1    -28 dB        4:1
============ ============ ====== ============= ======

Depth scales the whole effect; Upward and Down scale the two halves; Time
maps (exponentially) to attack 0.5-20 ms and release 20-400 ms.

Space
-----
Chorus
    Insert effect, one to four voices with sine LFOs, base delays 12-18.6 ms
    modulated by 3.5 ms, alternating stereo spread.
Reverb
    An 8-line feedback delay network with a Hadamard mixing matrix, per-line
    damping and three Schroeder all-pass diffusers.  Room: RT60 0.55 s;
    Hall: 1.8 s; Plate: 1.3 s.  Each type has its own pre-delay (Room 8 ms,
    Hall 18 ms, Plate 4 ms); the Pre-delay control adds 0-100 ms to it.  An
    optional low-cut (20 Hz-1 kHz) and high-cut (2-20 kHz), both 12
    dB/octave, shape the reverb signal only; at their ends of the range they
    are not run.
Delay
    Ping-pong, tempo-synced from the host (120 BPM when the host gives none),
    note lengths from 1/16 to 1/2 including triplets and dotted values,
    smoothly slewed when the tempo changes.  Feedback (0-90 %, default 40 %)
    is band-limited by a 150 Hz high-pass and a low-pass whose corner is the
    Tone control (1-16 kHz, default 4.5 kHz), so lower Tone makes the repeats
    darker.
Ducking
    A voice detector (4 ms attack, 180 ms release) lowers the reverb and the
    delay by their Ducking amount while you speak, so words stay clear.

Out Sat and limiter
-------------------
Saturation is a tanh with a small bias for even harmonics, made alias-free
to first order with antiderivative anti-aliasing, followed by DC removal.
The limiter has an instant attack (no lookahead, hence 0 ms latency), a
60 ms release and a -1 dBFS ceiling, and hard-clips at the ceiling as a
final guarantee.  Out gain is applied before the saturator and the limiter.

Latency summary
===============

=========================== =============================================
Condition                   Added latency
=========================== =============================================
Everything except Tune      0 samples
Tune on (VST2, VST3, LV2)   10 ms, reported to the host while on
Tune on (CLAP)              10 ms, *not* reported (see ``design.rst``)
Master off                  0 samples (raw bypass)
=========================== =============================================

Known limits
============

* Pitch correction is monophonic and needs a clearly voiced signal between
  70 and 900 Hz.  It follows the detector with a few milliseconds of lag, so
  fast glides are corrected a little late.
* The saturator is anti-aliased only to first order; very high drive on
  very bright material can still alias slightly.
* The limiter has no lookahead.  Extremely fast transients above the
  ceiling produce brief gain steps before the hard clip catches them.
* The reverb is a compact FDN, not a convolution or plate emulation.
