/* Exercise the production DSP fallback through its public frame entry point.
 * No guest assets, device, DSP initialization or host audio playback needed. */
#include "apu_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void xbox_SetAckTick(void (*fn)(void)) { (void)fn; }

static unsigned checks, failures;
static int front_only;
static MCPXAPUState *apu;
static float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME];

static void check_sample(const char *label, unsigned tick, unsigned sample,
                         unsigned channel, int expected)
{
    int actual = apu->monitor.frame_buf[tick * NUM_SAMPLES_PER_FRAME + sample][channel];
    ++checks;
    if (abs(actual - expected) > 1) {
        if (failures < 12)
            fprintf(stderr, "%s tick=%u sample=%u channel=%u: %d, expected %d\n",
                    label, tick, sample, channel, actual, expected);
        ++failures;
    }
}

static void clear_case(void)
{
    memset(mixbins, 0, sizeof(mixbins));
    memset(apu->monitor.frame_buf, 0, sizeof(apu->monitor.frame_buf));
    apu->monitor.ep_fifo_frame_valid = false;
    apu->monitor.point = MCPX_APU_DEBUG_MON_EP;
}

static void check_single_bus(void)
{
    /* Impulses independently identify each standard DirectSound dry bus.
     * The remaining bins are effect/submix inputs, not speaker outputs. */
    static const float expected_channels[10][2] = {
        {1, 0}, {0, 1}, {0.70710678f, 0.70710678f},
        {0.70710678f, 0.70710678f}, {1, 0}, {0, 1},
        {1, 0}, {0, 1}, {1, 0}, {0, 1}
    };
    for (unsigned bus = 0; bus < NUM_MIXBINS; ++bus) {
        for (unsigned tick = 0; tick < 8; ++tick) {
            clear_case();
            /* Distinct signed samples reveal swaps and accidental rectifying. */
            mixbins[bus][3] = 0.25f;
            mixbins[bus][17] = -0.5f;
            apu->ep_frame_div = tick;
            mcpx_apu_dsp_frame(apu, mixbins);
            for (unsigned epoch = 0; epoch < 8; ++epoch) {
                for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample) {
                    float input = epoch == tick ? mixbins[bus][sample] : 0;
                    for (unsigned ch = 0; ch < 2; ++ch) {
                        float gain = bus < 10 ? expected_channels[bus][ch] : 0;
                        if (front_only && bus > 1) gain = 0;
                        check_sample("single-bus impulse", epoch, sample, ch,
                                     (int)(input * gain * 32767.0f));
                    }
                }
            }
        }
    }
}

static void check_effect_sends_do_not_reverse_or_clip(void)
{
    for (unsigned side = 0; side < 2; ++side) {
        clear_case();
        for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample) {
            /* A source is quiet on one side while its reverb/effect sends are
             * loud. The dry image and headroom must be independent of those
             * sends. Old parity summing clipped both sides at full scale. */
            mixbins[6 + side][sample] = 0.25f;
            for (unsigned bus = 10; bus < NUM_MIXBINS; ++bus)
                mixbins[bus][sample] = sample & 1 ? -1.0f : 1.0f;
        }
        apu->ep_frame_div = 0;
        mcpx_apu_dsp_frame(apu, mixbins);
        for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample)
            for (unsigned ch = 0; ch < 2; ++ch)
                check_sample("dry source plus effects", 0, sample, ch,
                             !front_only && ch == side ? 8191 : 0);
    }
}

static void check_chronology_and_clipping(void)
{
    clear_case();
    for (unsigned tick = 0; tick < 8; ++tick) {
        for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample) {
            mixbins[0][sample] = (float)(tick * NUM_SAMPLES_PER_FRAME + sample) / 1024.0f;
            mixbins[1][sample] = -mixbins[0][sample];
        }
        apu->ep_frame_div = tick;
        mcpx_apu_dsp_frame(apu, mixbins);
    }
    for (unsigned tick = 0; tick < 8; ++tick)
        for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample)
            for (unsigned ch = 0; ch < 2; ++ch) {
                float value = (float)(tick * NUM_SAMPLES_PER_FRAME + sample) / 1024.0f;
                check_sample("chronological stereo", tick, sample, ch,
                             (int)(value * (ch ? -32767.0f : 32767.0f)));
            }

    clear_case();
    mixbins[0][0] = 2.0f;
    mixbins[1][0] = -2.0f;
    apu->ep_frame_div = 0;
    mcpx_apu_dsp_frame(apu, mixbins);
    check_sample("positive saturation", 0, 0, 0, 32767);
    check_sample("negative saturation", 0, 0, 1, -32767);
}

static void check_stereo_with_cross_parity_effect_sends(void)
{
    clear_case();
    /* A stereo assignment of [0,1,11,12] sends left to 0 and 11, right to
     * 1 and 12. Effect-bus parity is therefore the opposite of its source
     * channel. Playing those sends dry collapses the stereo image and can
     * double the amplitude. This layout is used by BLACK's guest. */
    for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample) {
        mixbins[0][sample] = mixbins[11][sample] = sample & 1 ? 0.6f : 0.125f;
        mixbins[1][sample] = mixbins[12][sample] = sample & 1 ? 0.6f : -0.25f;
    }
    apu->ep_frame_div = 0;
    mcpx_apu_dsp_frame(apu, mixbins);
    for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample) {
        check_sample("stereo with cross-parity sends", 0, sample, 0,
                     (int)((sample & 1 ? 0.6f : 0.125f) * 32767.0f));
        check_sample("stereo with cross-parity sends", 0, sample, 1,
                     (int)((sample & 1 ? 0.6f : -0.25f) * 32767.0f));
    }
}

static void check_native_and_vp_ownership(void)
{
    for (unsigned mode = 0; mode < 2; ++mode) {
        clear_case();
        for (unsigned i = 0; i < 256; ++i) {
            apu->monitor.frame_buf[i][0] = (int16_t)(1000 + i);
            apu->monitor.frame_buf[i][1] = (int16_t)(-2000 - i);
        }
        if (mode == 0) apu->monitor.ep_fifo_frame_valid = true;
        else apu->monitor.point = MCPX_APU_DEBUG_MON_VP;
        for (unsigned bus = 0; bus < NUM_MIXBINS; ++bus)
            for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample)
                mixbins[bus][sample] = 0.5f;
        for (unsigned tick = 0; tick < 8; ++tick) {
            apu->ep_frame_div = tick;
            mcpx_apu_dsp_frame(apu, mixbins);
        }
        for (unsigned tick = 0; tick < 8; ++tick)
            for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample) {
                int i = (int)(tick * NUM_SAMPLES_PER_FRAME + sample);
                check_sample("native/VP ownership", tick, sample, 0, 1000 + i);
                check_sample("native/VP ownership", tick, sample, 1, -2000 - i);
            }
    }
}

int main(void)
{
    const char *mode = getenv("APU_SPATIAL_FRONT_ONLY");
    front_only = mode && atoi(mode);
    apu = (MCPXAPUState *)calloc(1, sizeof(*apu));
    if (!apu) return 2;
    check_single_bus();
    check_effect_sends_do_not_reverse_or_clip();
    check_chronology_and_clipping();
    check_stereo_with_cross_parity_effect_sends();
    check_native_and_vp_ownership();
    free(apu);
    printf("APU spatial: %u checks, %u failures (%s).\n",
           checks, failures, front_only ? "front-only control" : "dry stereo");
    return failures ? 1 : 0;
}
