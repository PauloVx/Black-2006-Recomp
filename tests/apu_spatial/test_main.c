/* Exercise the production DSP fallback through its public frame entry point.
 * No guest assets, device, DSP initialization or host audio playback needed. */
#include "apu_state.h"
#include "apu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void xbox_SetAckTick(void (*fn)(void)) { (void)fn; }

static unsigned checks, failures;
static int front_only;
static MCPXAPUState *apu;
static float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME];

static void check(int condition, const char *label)
{
    ++checks;
    if (!condition) {
        if (failures < 12) fprintf(stderr, "%s\n", label);
        ++failures;
    }
}

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
    apu->fallback_limiter_gain = 1.0;
    apu->trace_output = false;
    apu->output_overloaded_samples = apu->output_limited_samples = 0;
    apu->output_clipped_samples = 0;
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

static void check_chronology(void)
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

}

static void check_limiter_overlap_and_release(void)
{
    clear_case();
    apu->trace_output = true;
    for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample) {
        /* Two individually valid buses overlap. The right channel is quiet
         * and must receive the same gain reduction as the overloaded left. */
        mixbins[0][sample] = front_only ? 2.0f : 1.0f;
        mixbins[4][sample] = 1.0f;
        mixbins[1][sample] = -0.5f;
    }
    apu->ep_frame_div = 0;
    mcpx_apu_dsp_frame(apu, mixbins);
    for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample) {
        check_sample("limited overlapping left", 0, sample, 0, 32767);
        check_sample("linked quiet right", 0, sample, 1, -8191);
    }
    check(apu->output_overloaded_samples == 32, "raw overload counts channel samples");
    check(apu->output_limited_samples == 64, "limiting counts both nonzero channels");
    check(apu->output_clipped_samples == 0, "limiting avoids actual hard clipping");

    memset(mixbins, 0, sizeof(mixbins));
    mixbins[0][0] = 0.5f;
    mixbins[1][0] = -0.125f;
    apu->ep_frame_div = 1;
    mcpx_apu_dsp_frame(apu, mixbins);
    int left = apu->monitor.frame_buf[32][0];
    int right = apu->monitor.frame_buf[32][1];
    check(left >= 8191 && left <= 8200, "gain reduction persists across a frame boundary");
    check(abs(left + 4 * right) <= 4, "release preserves the stereo amplitude ratio");

    /* Continue with silence to a total of 4800 release samples: a 100 ms
     * time constant restores 1 - 0.5/e of unity gain, even with zero PCM. */
    memset(mixbins, 0, sizeof(mixbins));
    for (unsigned tick = 1; tick < 150; ++tick) {
        apu->ep_frame_div = tick % 8;
        mcpx_apu_dsp_frame(apu, mixbins);
    }
    check(fabs(apu->fallback_limiter_gain - (1.0 - 0.5 / exp(1.0))) < 1e-9,
          "silence advances the 100 ms release across ticks");
    check(apu->output_clipped_samples == 0, "release requires no safety clipping");
    mcpx_apu_dsp_reset(apu);
    check(apu->fallback_limiter_gain == 1.0, "DSP reset restores unity gain");
    check(apu->output_overloaded_samples == 32, "DSP reset retains cumulative telemetry");
}

static void check_limiter_sustained_sine(void)
{
    clear_case();
    apu->trace_output = true;
    double output_square = 0, sine_projection = 0, cosine_projection = 0;
    unsigned observed = 0, plateau = 0, max_plateau = 0;
    const double tau = 6.2831853071795864769;
    for (unsigned tick = 0; tick < 1500; ++tick) {
        for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample) {
            unsigned n = tick * NUM_SAMPLES_PER_FRAME + sample;
            /* Start at a peak; discard the first two periods for waveform
             * quality. This is a 1 kHz tone from two unity-amplitude sources. */
            float value = (float)cos(tau * n / 48.0);
            mixbins[0][sample] = (front_only ? 2.0f : 1.0f) * value;
            mixbins[4][sample] = value;
            mixbins[1][sample] = (front_only ? 0.5f : 0.25f) * value;
            mixbins[5][sample] = 0.25f * value;
        }
        apu->ep_frame_div = tick % 8;
        mcpx_apu_dsp_frame(apu, mixbins);
        for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample) {
            unsigned n = tick * NUM_SAMPLES_PER_FRAME + sample;
            int left = apu->monitor.frame_buf[(tick % 8) * 32 + sample][0];
            int right = apu->monitor.frame_buf[(tick % 8) * 32 + sample][1];
            check(abs(left - 4 * right) <= 4, "sustained overload retains stereo ratio");
            if (n < 96) continue;
            if (abs(left) >= 32766) ++plateau;
            else plateau = 0;
            if (plateau > max_plateau) max_plateau = plateau;
            double value = left / 32767.0;
            output_square += value * value;
            sine_projection += value * sin(tau * n / 48.0);
            cosine_projection += value * cos(tau * n / 48.0);
            ++observed;
        }
    }
    /* Project the output onto its fundamental. Independent channel clamps
     * leave long full-scale plateaus and strong harmonics, unlike the linked
     * gain envelope. This measures output quality, not the implementation. */
    double fundamental = 2.0 * (sine_projection * sine_projection +
        cosine_projection * cosine_projection) / ((double)observed * observed);
    double residual = fmax(0.0, output_square / observed - fundamental);
    printf("APU limiter tone: residual-rms=%.8f max-plateau=%u raw-overloaded=%llu limited=%llu clipped=%llu.\n",
           sqrt(residual), max_plateau,
           (unsigned long long)apu->output_overloaded_samples,
           (unsigned long long)apu->output_limited_samples,
           (unsigned long long)apu->output_clipped_samples);
    check(max_plateau <= 1, "sustained tone has no hard-clipping plateaus");
    check(sqrt(residual) < 0.003, "sustained tone residual stays below 0.3 percent full scale");
    check(apu->output_overloaded_samples > 1000, "tone exercises genuine raw overload");
    check(apu->output_limited_samples > 1000, "tone exercises gain reduction");
    check(apu->output_clipped_samples == 0, "tone produces no post-limit saturation");
}

static void check_limiter_finite_inputs(void)
{
    clear_case();
    mixbins[0][0] = NAN;
    mixbins[1][0] = 0.25f;
    mixbins[0][1] = INFINITY;
    mixbins[1][1] = -INFINITY;
    apu->fallback_limiter_gain = NAN;
    apu->ep_frame_div = 0;
    mcpx_apu_dsp_frame(apu, mixbins);
    check_sample("nonfinite left is silent", 0, 0, 0, 0);
    check_sample("finite opposite channel is preserved", 0, 0, 1, 8191);
    check_sample("infinite left is silent", 0, 1, 0, 0);
    check_sample("infinite right is silent", 0, 1, 1, 0);
    check(isfinite(apu->fallback_limiter_gain), "invalid state recovers finite gain");
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
        apu->fallback_limiter_gain = 0.5;
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
        check(apu->fallback_limiter_gain == 1.0, "native/VP bypass clears old fallback gain");
    }
}

static void check_configured_stereo_bus(void)
{
    /* BLACK's primary custom bus is a stereo pair at 19/20. A voice can
     * route exclusively there, with all standard speaker sends muted. */
    ++checks;
    if (!mcpx_apu_set_stereo_fallback_pair(apu, 19, 20)) ++failures;
    for (unsigned tick = 0; tick < 8; ++tick) {
        clear_case();
        for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample) {
            mixbins[19][sample] = sample & 1 ? -0.5f : 0.25f;
            mixbins[20][sample] = sample & 1 ? 0.125f : -0.375f;
            /* Auxiliary sends must not become extra dry speakers. */
            mixbins[11][sample] = mixbins[12][sample] = 1.0f;
            for (unsigned bus = 21; bus < NUM_MIXBINS; ++bus)
                mixbins[bus][sample] = -1.0f;
        }
        apu->ep_frame_div = tick;
        mcpx_apu_dsp_frame(apu, mixbins);
        for (unsigned epoch = 0; epoch < 8; ++epoch)
            for (unsigned sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample)
                for (unsigned ch = 0; ch < 2; ++ch) {
                    float value = epoch == tick && !front_only ? mixbins[19 + ch][sample] : 0;
                    check_sample("custom stereo bus", epoch, sample, ch,
                                 (int)(value * 32767.0f));
                }
    }

    /* Invalid requests leave the previous pair intact. */
    static const unsigned invalid[][2] = {{9,20}, {19,9}, {32,20}, {19,32}, {19,19}};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        ++checks;
        if (mcpx_apu_set_stereo_fallback_pair(apu, invalid[i][0], invalid[i][1])) ++failures;
    }
    ++checks;
    if (mcpx_apu_set_stereo_fallback_pair(NULL, 19, 20)) ++failures;
    clear_case();
    mixbins[0][0] = 0.125f;
    mixbins[1][0] = -0.0625f;
    mixbins[19][0] = 0.25f;
    mixbins[20][0] = -0.125f;
    apu->ep_frame_div = 0;
    mcpx_apu_dsp_frame(apu, mixbins);
    check_sample("custom plus standard left", 0, 0, 0,
                 (int)((front_only ? 0.125f : 0.375f) * 32767.0f));
    check_sample("custom plus standard right", 0, 0, 1,
                 (int)((front_only ? -0.0625f : -0.1875f) * 32767.0f));

    /* Direction comes from configuration, including an even left / even
     * right pair. It cannot be inferred from numerical bin parity. */
    ++checks;
    if (!mcpx_apu_set_stereo_fallback_pair(apu, 30, 10)) ++failures;
    clear_case();
    mixbins[30][4] = 0.25f;
    mixbins[10][4] = -0.5f;
    apu->ep_frame_div = 0;
    mcpx_apu_dsp_frame(apu, mixbins);
    check_sample("explicit left bin", 0, 4, 0, front_only ? 0 : 8191);
    check_sample("explicit right bin", 0, 4, 1, front_only ? 0 : -16383);
    ++checks;
    if (!mcpx_apu_set_stereo_fallback_pair(apu, 19, 20)) ++failures;
}

int main(void)
{
    const char *mode = getenv("APU_SPATIAL_FRONT_ONLY");
    front_only = mode && atoi(mode);
    apu = (MCPXAPUState *)calloc(1, sizeof(*apu));
    if (!apu) return 2;
    check_single_bus();
    check_effect_sends_do_not_reverse_or_clip();
    check_chronology();
    check_stereo_with_cross_parity_effect_sends();
    check_configured_stereo_bus();
    printf("APU dry routing: %u checks, %u failures.\n", checks, failures);
    check_native_and_vp_ownership();
    check_limiter_overlap_and_release();
    check_limiter_sustained_sine();
    check_limiter_finite_inputs();
    free(apu);
    printf("APU spatial: %u checks, %u failures (%s).\n",
           checks, failures, front_only ? "front-only control" : "dry stereo");
    return failures ? 1 : 0;
}
