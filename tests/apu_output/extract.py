import argparse
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--core-source',type=Path,required=True)
p.add_argument('--core-out',type=Path,required=True)
a=p.parse_args()
s=a.source.read_text()
start=s.index('#define XA2_SAMPLE_RATE')
end=s.index('\n#else /* !_WIN32',start)
a.out.write_text(s[start:end])
core=a.core_source.read_text()
start=core.index('static void *mcpx_apu_frame_thread(void *arg)')
end=core.index('\n/* ============================================================',start)
a.core_out.write_text(core[start:end])
# Startup must wait for the owner thread's initialization, and shutdown must
# join its finalization. Reject a return to caller-thread COM ownership.
init=core[core.index('MCPXAPUState *mcpx_apu_init_standalone_sized('):core.index('\nvoid mcpx_apu_shutdown(')]
shutdown=core[core.index('\nvoid mcpx_apu_shutdown('):core.index('\n/* ============================================================',core.index('\nvoid mcpx_apu_shutdown('))]
assert 'mcpx_apu_monitor_init(' not in init
assert 'mcpx_apu_monitor_finalize(' not in shutdown
assert init.index('qemu_thread_create(')<init.index('mcpx_apu_wait_for_idle(')<init.index('qemu_mutex_unlock(')
assert shutdown.index('qemu_thread_join(')<shutdown.index('free(d)')
