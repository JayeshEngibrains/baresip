#ifndef TEST_AUDIO_H
#define TEST_AUDIO_H

#include <stdint.h>
#include <stddef.h>

/*
 * PCM Audio Data
 * Source WAV File : testaudio_8000_test01_20s.wav
 *
 * Sample Rate     : 8000 Hz
 * Channels        : 1
 * Bit Depth       : 16 bits
 * PCM Format      : Signed 16-bit Little Endian
 * Duration        : 24.000 seconds
 * Total Samples   : 192000
 * Total Bytes     : 384000
 */

extern const int16_t test_audio_pcm[];
extern const size_t test_audio_pcm_samples;

#define TEST_AUDIO_PCM_SAMPLE_RATE   8000
#define TEST_AUDIO_PCM_CHANNELS      1
#define TEST_AUDIO_PCM_BIT_DEPTH     16

#endif /* TEST_AUDIO_H */
