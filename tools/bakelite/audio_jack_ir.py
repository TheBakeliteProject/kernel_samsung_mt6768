#!/usr/bin/env python3
"""Create a stereo NEC consumer-IR waveform for an external audio-jack emitter."""

import argparse
import math
import struct
import wave


def nec_segments(code, repeats):
    segments = [(True, 9000), (False, 4500)]
    for bit in range(32):
        segments.append((True, 562))
        segments.append((False, 1687 if code & (1 << bit) else 562))
    segments.append((True, 562))
    for _ in range(repeats):
        segments.extend(((False, 40000), (True, 9000),
                         (False, 2250), (True, 562)))
    segments.append((False, 40000))
    return segments


def write_wave(path, segments, sample_rate, carrier):
    pcm = bytearray()
    for mark, usec in segments:
        frames = round(sample_rate * usec / 1000000)
        for frame in range(frames):
            value = (round(26000 * math.sin(2 * math.pi * carrier * frame /
                                            sample_rate)) if mark else 0)
            pcm.extend(struct.pack('<hh', value, -value))

    with wave.open(path, 'wb') as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(pcm)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('code', type=lambda value: int(value, 0),
                        help='32-bit NEC code, e.g. 0x20DF10EF')
    parser.add_argument('output', help='output WAV file')
    parser.add_argument('--repeat', type=int, default=0,
                        help='number of NEC repeat frames (0-5)')
    parser.add_argument('--carrier', type=int, default=19000,
                        help='audio carrier in Hz (default: 19000)')
    args = parser.parse_args()

    if not 0 <= args.code <= 0xffffffff:
        parser.error('code must fit in 32 bits')
    if not 0 <= args.repeat <= 5:
        parser.error('repeat must be between 0 and 5')
    if not 1000 <= args.carrier < 24000:
        parser.error('carrier must be between 1000 and 23999 Hz')

    write_wave(args.output, nec_segments(args.code, args.repeat),
               48000, args.carrier)


if __name__ == '__main__':
    main()
