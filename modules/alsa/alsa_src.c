/**
 * @file alsa_src.c  ALSA sound driver - recorder
 *
 * Copyright (C) 2010 Alfred E. Heggestad
 */
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <re_atomic.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "alsa.h"
#include "test_audio.h"


struct ausrc_st {
	thrd_t thread;
	RE_ATOMIC bool run;
	snd_pcm_t *read;
	void *sampv;
	size_t sampc;
	ausrc_read_h *rh;
	void *arg;
	struct ausrc_prm prm;
	char *device;
	size_t pcm_index;
};


static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;

	/* Wait for termination of other thread */
	if (re_atomic_rlx(&st->run)) {
		debug("alsa: stopping recording thread (%s)\n", st->device);
		re_atomic_rlx_set(&st->run, false);
		thrd_join(st->thread, NULL);
	}

	/* No ALSA to close */

	mem_deref(st->sampv);
	mem_deref(st->device);
}


static int read_thread(void *arg)
{
	struct ausrc_st *st = arg;
	uint64_t frames = 0;
	int num_frames;
	int err = 0;

	warning("alsa_src: read_thread started\n");

	num_frames = st->prm.srate * st->prm.ptime / 1000;

	while (re_atomic_rlx(&st->run)) {
		struct auframe af;

		/* Copy PCM data from test_audio */
		size_t bytes_to_copy = num_frames * sizeof(int16_t);
		
		/* Check if we have reached the end of the test audio */
		if (st->pcm_index + num_frames > 192000) {
			warning("alsa_src: End of test audio reached. Stopping stream.\n");
			
			/* Send remaining frames if any */
			if (st->pcm_index < 192000) {
				size_t remaining = 192000 - st->pcm_index;
				memcpy(st->sampv, &test_audio_pcm[st->pcm_index], remaining * sizeof(int16_t));
				
				auframe_init(&af, st->prm.fmt, st->sampv, remaining * st->prm.ch,
					     st->prm.srate, st->prm.ch);
				af.timestamp = frames * AUDIO_TIMEBASE / st->prm.srate;
				frames += remaining;
				st->rh(&af, st->arg);
			}
			
			/* Stop the thread loop */
			re_atomic_rlx_set(&st->run, false);
			
			/* Trigger hangup - this is a hack, ideally we should signal the application */
			/* Since we don't have easy access to UA here, we'll just stop sending audio */
			/* The user will have to manually hang up or we can try to use a global event if available */
			
			/* Try to print stats here */
			warning("alsa_src: Total frames sent: %llu\n", frames);
			
			break;
		}

		memcpy(st->sampv, &test_audio_pcm[st->pcm_index], bytes_to_copy);
		st->pcm_index += num_frames;

		debug("alsa_src: copied %d frames from test_audio, index now %zu, first sample: %d\n", 
		      num_frames, st->pcm_index, ((int16_t*)st->sampv)[0]);

		auframe_init(&af, st->prm.fmt, st->sampv, num_frames * st->prm.ch,
			     st->prm.srate, st->prm.ch);
		af.timestamp = frames * AUDIO_TIMEBASE / st->prm.srate;

		frames += num_frames;

		debug("alsa_src: sending auframe with %d samples, timestamp %llu\n", af.sampc, af.timestamp);

		st->rh(&af, st->arg);

		/* Simulate timing - sleep for ptime ms */
		sys_msleep(st->prm.ptime);
	}

	return err;
}


int alsa_src_alloc(struct ausrc_st **stp, const struct ausrc *as,
		   struct ausrc_prm *prm, const char *device,
		   ausrc_read_h *rh, ausrc_error_h *errh, void *arg)
{
	struct ausrc_st *st;
	int err;
	(void)errh;

	warning("alsa_src_alloc: called with srate=%d, ch=%d, fmt=%s, device=%s\n",
		prm->srate, prm->ch, aufmt_name(prm->fmt), device ? device : "NULL");

	if (!stp || !as || !prm || !rh)
		return EINVAL;

	if (!str_isset(device))
		device = alsa_dev;

	st = mem_zalloc(sizeof(*st), ausrc_destructor);
	if (!st)
		return ENOMEM;

	err = str_dup(&st->device, device);
	if (err)
		goto out;

	st->prm = *prm;
	st->rh  = rh;
	st->arg = arg;
	st->pcm_index = 0;

	st->sampc = prm->srate * prm->ch * prm->ptime / 1000;

	st->sampv = mem_alloc(aufmt_sample_size(prm->fmt) * st->sampc, NULL);
	if (!st->sampv) {
		err = ENOMEM;
		goto out;
	}

	/* No ALSA setup needed for test data */

	re_atomic_rlx_set(&st->run, true);
	err = thread_create_name(&st->thread, "alsa_src", read_thread, st);
	if (err) {
		re_atomic_rlx_set(&st->run, false);
		goto out;
	}

	warning("alsa_src: thread started for test data, srate=%d, ch=%d, ptime=%d\n", 
		st->prm.srate, st->prm.ch, st->prm.ptime);

	debug("alsa: recording started (test data) (%s) format=%s\n",
	      st->device, aufmt_name(prm->fmt));

 out:
	if (err)
		mem_deref(st);
	else
		*stp = st;

	return err;
}
