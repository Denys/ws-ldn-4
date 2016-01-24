#include <math.h>
#include <string.h>
#include "ex04/audio_play.h"
#include "synth/synth.h"
#include "synth/adsr.h"
#include "synth/biquad.h"
#include "synth/panning.h"
#include "synth/osc.h"
#include "synth/node_ops.h"

#define VOLUME 50
// 256 BYTES
#define AUDIO_DMA_BUFFER_SIZE 512
// in 16bit words
#define AUDIO_DMA_BUFFER_SIZE2 (AUDIO_DMA_BUFFER_SIZE >> 1)
// half of the buffer filled each time
#define AUDIO_DMA_BUFFER_SIZE4 (AUDIO_DMA_BUFFER_SIZE >> 2)
// another half because of stereo
#define AUDIO_DMA_BUFFER_SIZE8 (AUDIO_DMA_BUFFER_SIZE >> 3)

typedef enum {
	BUFFER_OFFSET_NONE = 0, BUFFER_OFFSET_HALF, BUFFER_OFFSET_FULL
} DMABufferState;

__IO DMABufferState bufferState = BUFFER_OFFSET_NONE;

static CT_Synth synth;

static uint8_t audioBuf[AUDIO_DMA_BUFFER_SIZE];

static void initSynth();
static void updateAudioBuffer();

void demoAudioPlayback(void) {
	BSP_LED_On(LED_GREEN);
	memset(audioBuf, 0, AUDIO_DMA_BUFFER_SIZE);
	if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, VOLUME, SAMPLE_RATE) != 0) {
		Error_Handler();
	}

	BSP_AUDIO_OUT_Play((uint16_t *) audioBuf, AUDIO_DMA_BUFFER_SIZE);

	initSynth();

	while (1) {
		updateAudioBuffer();
	}

//	if (BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW) != AUDIO_OK) {
//		Error_Handler();
//	}
}

static void initSynth() {
	ct_synth_init(&synth, 1);
	synth.lfo[0] =
	        ct_synth_osc("lfo1", ct_synth_process_osc_sin, PI * 1.25f,
	                     HZ_TO_RAD(1 / 24.0f), 0.6f, 0.9f);
	synth.numLFO = 1;

	float freq = 110.0f;
	CT_DSPNode *env = ct_synth_adsr("env", synth.lfo[0], 0.005f, 0.05f, 0.02f,
			1.0f, 0.99f);
	CT_DSPNode *osc1 = ct_synth_osc("osc1", ct_synth_process_osc_saw, 0.0f,
			freq, 0.3f, 0.0f);
	CT_DSPNode *osc2 = ct_synth_osc("osc2", ct_synth_process_osc_saw, 0.0f,
			freq, 0.3f, 0.0f);
	CT_DSPNode *sum = ct_synth_op4("sum", osc1, env, osc2, env,
			ct_synth_process_madd);
	//CT_DSPNode *filter = ct_synth_filter_biquad("filter", LPF, sum, 100.0f,
	//		3.0f, 0.5f);
	CT_DSPNode *pan = ct_synth_panning("stereo", osc1, NULL, 0.5);
	//CT_DSPNode *delay = ct_synth_delay("delay", pan,
	//		(uint32_t) (SAMPLE_RATE * 3 / 8), 0.2f, 2);
	CT_DSPNode *nodes[] = { osc1, pan };
	ct_synth_build_stack(&synth.stacks[0], nodes, 2);
}

static void updateAudioBuffer() {
	if (bufferState == BUFFER_OFFSET_HALF) {
		int16_t *ptr = (int16_t*) &audioBuf;
		ct_synth_update_mix_stereo_i16(&synth, AUDIO_DMA_BUFFER_SIZE4, ptr);
		bufferState = BUFFER_OFFSET_NONE;
	}

	if (bufferState == BUFFER_OFFSET_FULL) {
		int16_t *ptr = (int16_t*) &audioBuf[AUDIO_DMA_BUFFER_SIZE2];
		ct_synth_update_mix_stereo_i16(&synth, AUDIO_DMA_BUFFER_SIZE4, ptr);
		bufferState = BUFFER_OFFSET_NONE;
	}
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void) {
	bufferState = BUFFER_OFFSET_HALF;
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void) {
	bufferState = BUFFER_OFFSET_FULL;
	BSP_AUDIO_OUT_ChangeBuffer((uint16_t*) &audioBuf,
	AUDIO_DMA_BUFFER_SIZE2);
}

void BSP_AUDIO_OUT_Error_CallBack(void) {
	Error_Handler();
}
