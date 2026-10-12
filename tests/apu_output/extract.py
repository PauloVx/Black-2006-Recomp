import argparse
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--core-source',type=Path,required=True)
p.add_argument('--core-out',type=Path,required=True)
p.add_argument('--delivery-out',type=Path,required=True)
p.add_argument('--wait-out',type=Path,required=True)
p.add_argument('--mixer-out',type=Path,required=True)
p.add_argument('--unavailable-out',type=Path,required=True)
a=p.parse_args()
s=a.source.read_text()
start=s.index('#define XA2_SAMPLE_RATE')
end=s.index('\n#else /* !_WIN32',start)
a.out.write_text(s[start:end])
core=a.core_source.read_text()
start=core.index('static void *mcpx_apu_frame_thread(void *arg)')
end=core.index('\n/* ============================================================',start)
a.core_out.write_text(core[start:end])
tail_marker='/* A one-shot software-mixer voice can finish between output packets.'
monitor=core[core.index('void mcpx_apu_monitor_frame('):core.index('\nstatic void throttle(')]
if tail_marker in core:
    start=core.index(tail_marker)
    end=core.index('\nvoid mcpx_apu_monitor_init(',start)
    a.wait_out.write_text('#define APU_OUTPUT_HAS_MIXER_TAIL 1\n'+core[start:end])
    a.delivery_out.write_text('    monitor_submit_packet(d, packet_samples, 0);\n')
else:
    a.wait_out.write_text('/* Pre-correction monitor has no mixer-tail drain. */\n')
    # Keep the old one-shot delivery path compilable as a negative control.
    start=monitor.index('    if (xa2_is_active()) {', monitor.index('g_monitor_packet.used += MIXER_FRAME_SAMPLES;'))
    end=monitor.index('    } else {',start)
    a.delivery_out.write_text(monitor[start+len('    if (xa2_is_active()) {'):end])
    submit_marker='static int monitor_submit_xaudio2('
    if submit_marker in core:
        start=core.index(submit_marker)
        end=core.index('\nvoid mcpx_apu_monitor_init(',start)
        a.wait_out.write_text(core[start:end])
unavailable_marker='    if ((!xa2_is_active() && !g_waveout.initialized) ||'
start=monitor.index(unavailable_marker)
brace=monitor.index('{',start)
depth=0
for end in range(brace,len(monitor)):
    if monitor[end]=='{': depth+=1
    elif monitor[end]=='}':
        depth-=1
        if depth==0: break
a.unavailable_out.write_text(monitor[start:end+1])
mixer_marker='static void mixer_render('
if mixer_marker in core:
    a.mixer_out.write_text(core[core.rindex(mixer_marker):])
else:
    a.mixer_out.write_text('/* Source has no software mixer renderer. */\n')
# Startup must wait for the owner thread's initialization, and shutdown must
# join its finalization. Reject a return to caller-thread COM ownership.
init=core[core.index('MCPXAPUState *mcpx_apu_init_standalone_sized('):core.index('\nvoid mcpx_apu_shutdown(')]
shutdown=core[core.index('\nvoid mcpx_apu_shutdown('):core.index('\n/* ============================================================',core.index('\nvoid mcpx_apu_shutdown('))]
assert 'mcpx_apu_monitor_init(' not in init
assert 'mcpx_apu_monitor_finalize(' not in shutdown
assert init.index('qemu_thread_create(')<init.index('mcpx_apu_wait_for_idle(')<init.index('qemu_mutex_unlock(')
assert shutdown.index('qemu_thread_join(')<shutdown.index('free(d)')
