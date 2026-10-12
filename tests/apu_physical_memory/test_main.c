/* Actual physical-memory getters and actual DSP reset/MMIO bootstrap. No
 * retail data, DSP execution, audio device, or guest threads are required. */
#include "apu_state.h"
#include "apu_dma_regs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define XBOX_CONTIG_BASE 0x80000000u
#define XBOX_CONTIG_SIZE (64u * 1024u * 1024u)
#define XBOX_LOWRAM_PHYS_VA (XBOX_CONTIG_BASE + XBOX_CONTIG_SIZE)
#define XBOX_NUM_MIRRORS 28
static void *g_contig_memory, *g_lowram_phys_view;
static size_t g_memory_size;
#include "physical_memory.inc"

void xbox_SetAckTick(void (*fn)(void)) { (void)fn; }
void mcpx_apu_dsp_mmio_write(MCPXAPUState *d, hwaddr addr, uint64_t val,
                            unsigned int size, bool is_gp);

static unsigned checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    fprintf(stderr, "Failed line %u: %s\n", (unsigned)__LINE__, #x); } } while (0)

static void check_getters(uint8_t *ram)
{
    g_memory_size = XBOX_CONTIG_SIZE;
    CHECK(xbox_GetPhysicalMemoryBase() == NULL);
    CHECK(xbox_GetPhysicalMemorySize() == 0);
    g_lowram_phys_view = ram + XBOX_CONTIG_SIZE;
    CHECK(xbox_GetPhysicalMemorySize() == 0); /* No contiguous arena. */
    g_contig_memory = ram;
    g_lowram_phys_view = NULL;
    CHECK(xbox_GetPhysicalMemoryBase() == ram);
    CHECK(xbox_GetPhysicalMemorySize() == XBOX_CONTIG_SIZE);
    CHECK(xbox_PhysicalAddressOf(0x00123000u) == 0x00123000u);
    g_lowram_phys_view = ram + XBOX_CONTIG_SIZE;
    CHECK(xbox_GetPhysicalMemorySize() == 2u * XBOX_CONTIG_SIZE);
    CHECK(xbox_PhysicalAddressOf(0x00123000u) == XBOX_CONTIG_SIZE + 0x00123000u);
    CHECK(xbox_PhysicalAddressOf(0x80123000u) == 0x00123000u);
    CHECK(xbox_PhysicalAddressOf(0x84123000u) == XBOX_CONTIG_SIZE + 0x00123000u);
    /* A nonadjacent view cannot extend a single device backing pointer. */
    g_lowram_phys_view = ram + XBOX_CONTIG_SIZE + 4096;
    CHECK(xbox_GetPhysicalMemorySize() == XBOX_CONTIG_SIZE);
    g_lowram_phys_view = ram + XBOX_CONTIG_SIZE;
    g_memory_size = SIZE_MAX;
    CHECK(xbox_GetPhysicalMemorySize() == XBOX_CONTIG_SIZE);
    g_memory_size = XBOX_CONTIG_SIZE;
}

static void bootstrap(MCPXAPUState *apu, bool gp, uint32_t table,
                      uint32_t page0, uint32_t page1, bool accepted)
{
    DSPState *dsp = gp ? apu->gp.dsp : apu->ep.dsp;
    uint32_t table_reg = gp ? NV_PAPU_GPSADDR : NV_PAPU_EPSADDR;
    uint32_t max_reg = gp ? NV_PAPU_GPSMAXSGE : NV_PAPU_EPSMAXSGE;
    const uint32_t enabled = NV_PAPU_GPRST_GPRST | NV_PAPU_GPRST_GPDSPRST;
    uint32_t sentinel = 0x00c0ffeeu;
    for (unsigned i = 0; i < DSP_PRAM_SIZE; ++i) dsp->core.pram[i] = sentinel;
    memcpy(apu->ram_ptr + table, &page0, sizeof(page0));
    memcpy(apu->ram_ptr + table + 8, &page1, sizeof(page1));
    apu->regs[table_reg] = table;
    apu->regs[max_reg] = 1;
    mcpx_apu_dsp_mmio_write(apu, NV_PAPU_GPRST, 0, 4, gp);
    mcpx_apu_dsp_mmio_write(apu, NV_PAPU_GPRST, enabled, 4, gp);
    for (unsigned i = 0; i < 0x800; ++i) {
        uint32_t expected = sentinel;
        if (accepted) memcpy(&expected, apu->ram_ptr +
            (i < 0x400 ? page0 + i * 4 : page1 + (i - 0x400) * 4), 4);
        CHECK(dsp->core.pram[i] == (expected & 0x00ffffffu));
    }
    CHECK(dsp->core.pram[0x800] == sentinel);
}

static void check_dma(MCPXAPUState *apu, bool gp, bool fifo,
                      bool upper_table, bool upper_view)
{
    DSPState *dsp = gp ? apu->gp.dsp : apu->ep.dsp;
    const uint32_t page = XBOX_CONTIG_SIZE + 0x30000u;
    const uint32_t table = 0x18000u + (upper_table ? XBOX_CONTIG_SIZE : 0);
    const uint32_t table_reg = fifo ? (gp ? NV_PAPU_GPFADDR : NV_PAPU_EPFADDR)
                                    : (gp ? NV_PAPU_GPSADDR : NV_PAPU_EPSADDR);
    const uint32_t max_reg = fifo ? (gp ? NV_PAPU_GPFMAXSGE : NV_PAPU_EPFMAXSGE)
                                  : (gp ? NV_PAPU_GPSMAXSGE : NV_PAPU_EPSMAXSGE);
    const uint32_t base_reg = gp ? NV_PAPU_GPOFBASE0 : NV_PAPU_EPOFBASE0;
    const uint32_t end_reg = gp ? NV_PAPU_GPOFEND0 : NV_PAPU_EPOFEND0;
    const uint32_t cur_reg = gp ? NV_PAPU_GPOFCUR0 : NV_PAPU_EPOFCUR0;
    const uint32_t input_base = gp ? NV_PAPU_GPIFBASE0 : NV_PAPU_EPIFBASE0;
    const uint32_t input_end = gp ? NV_PAPU_GPIFEND0 : NV_PAPU_EPIFEND0;
    const uint32_t input_cur = gp ? NV_PAPU_GPIFCUR0 : NV_PAPU_EPIFCUR0;
    int16_t expected[512];
    uint32_t descriptor[7] = {
        NODE_POINTER_EOL,
        NODE_CONTROL_DIRECTION | ((fifo ? 0u : 0xFu) << 5) |
            (1u << 10) | (1u << 14),
        512, 0x100, 0, 0, 0xFFF
    };
    g_lowram_phys_view = upper_view ? apu->ram_ptr + XBOX_CONTIG_SIZE : NULL;
    apu->ram_size = xbox_GetPhysicalMemorySize();
    apu->regs[table_reg] = table;
    apu->regs[max_reg] = 0;
    memcpy(apu->ram_ptr + table, &page, sizeof(page));
    apu->regs[base_reg] = apu->regs[cur_reg] = 0;
    apu->regs[end_reg] = 0x1000;
    memset(apu->ram_ptr + page, 0x5a, sizeof(expected));
    memcpy(dsp->core.xram, descriptor, sizeof(descriptor));
    for (unsigned i = 0; i < 512; ++i) {
        expected[i] = (int16_t)((i & 1) ? -12000 + (int)i : 1000 + (int)i);
        dsp->core.xram[0x100 + i] = (uint32_t)(uint16_t)expected[i] << 8;
    }
    dsp->ep_fifo_frame_ready = false;
    dsp->ep_fifo_frame_bytes_used = 0;
    apu->monitor.ep_fifo_frame_valid = false;
    apu->monitor.point = MCPX_APU_DEBUG_MON_EP;
    dsp_dma_reset(&dsp->dma);
    dsp_dma_write(&dsp->dma, DMA_NEXT_BLOCK, 0);
    dsp_dma_write(&dsp->dma, DMA_CONTROL, DMA_CONTROL_ACTION_START);
    CHECK(dsp->dma.error == !upper_view);
    CHECK(!!(dsp->dma.pending_interrupts & DMA_INTERRUPT_ERROR) == !upper_view);
    if (upper_view) {
        CHECK(memcmp(apu->ram_ptr + page, expected, sizeof(expected)) == 0);
        if (fifo) CHECK(apu->regs[cur_reg] == sizeof(expected));
        if (!gp && fifo) {
            CHECK(dsp->ep_fifo_frame_ready);
            CHECK(apu->monitor.ep_fifo_frame_valid);
            CHECK(memcmp(apu->monitor.frame_buf, expected, sizeof(expected)) == 0);
        }
    } else {
        for (unsigned i = 0; i < sizeof(expected); ++i)
            CHECK(apu->ram_ptr[page + i] == 0x5a);
        if (fifo) CHECK(apu->regs[cur_reg] == 0);
        if (!gp && fifo) CHECK(!apu->monitor.ep_fifo_frame_valid);
    }

    /* Read the independent PCM into DSP X memory through the same real
     * descriptor engine and the production scratch/FIFO SGE callbacks. */
    memcpy(apu->ram_ptr + page, expected, sizeof(expected));
    descriptor[1] &= ~NODE_CONTROL_DIRECTION;
    memcpy(dsp->core.xram, descriptor, sizeof(descriptor));
    for (unsigned i = 0; i < 512; ++i) dsp->core.xram[0x100 + i] = 0x654321;
    apu->regs[input_base] = apu->regs[input_cur] = 0;
    apu->regs[input_end] = 0x1000;
    dsp_dma_reset(&dsp->dma);
    dsp_dma_write(&dsp->dma, DMA_NEXT_BLOCK, 0);
    dsp_dma_write(&dsp->dma, DMA_CONTROL, DMA_CONTROL_ACTION_START);
    CHECK(dsp->dma.error == !upper_view);
    for (unsigned i = 0; i < 512; ++i)
        CHECK(dsp->core.xram[0x100 + i] ==
            (upper_view ? (uint32_t)(uint16_t)expected[i] << 8 : 0x654321u));
    if (fifo) CHECK(apu->regs[input_cur] == (upper_view ? sizeof(expected) : 0));
}

int main(void)
{
    uint8_t *ram = calloc(2u * XBOX_CONTIG_SIZE, 1);
    MCPXAPUState *apu = calloc(1, sizeof(*apu));
    if (!ram || !apu) return 2;
    check_getters(ram);
    apu->ram_ptr = xbox_GetPhysicalMemoryBase();
    qemu_mutex_init(&apu->lock);
    mcpx_apu_dsp_init(apu);
    if (!apu->gp.dsp || !apu->ep.dsp || !apu->gp.dsp->engine_ready ||
        !apu->ep.dsp->engine_ready) return 2;
    /* Distinct 24-bit words prove both physical banks and both source pages
     * are read, and that guest word padding is masked during DSP loading. */
    for (unsigned bank = 0; bank < 2; ++bank)
        for (unsigned i = 0; i < 0x800; ++i) {
            uint32_t word = 0xab000000u | (bank << 20) | (0x10000u + i);
            memcpy(ram + bank * XBOX_CONTIG_SIZE + 0x20000u + i * 4, &word, 4);
        }
    for (unsigned gp = 0; gp < 2; ++gp) {
        g_lowram_phys_view = NULL;
        apu->ram_size = xbox_GetPhysicalMemorySize();
        bootstrap(apu, gp != 0, 0x10000u, 0x20000u, 0x21000u, true);
        /* Baseline 64-MB contract rejects source pages and SGE tables in
         * the low-RAM bank, despite their real physical addresses. */
        bootstrap(apu, gp != 0, 0x10000u, XBOX_CONTIG_SIZE + 0x20000u,
                  XBOX_CONTIG_SIZE + 0x21000u, false);
        bootstrap(apu, gp != 0, XBOX_CONTIG_SIZE + 0x10000u,
                  0x20000u, 0x21000u, false);
        g_lowram_phys_view = ram + XBOX_CONTIG_SIZE;
        apu->ram_size = xbox_GetPhysicalMemorySize();
        bootstrap(apu, gp != 0, XBOX_CONTIG_SIZE + 0x10000u,
                  XBOX_CONTIG_SIZE + 0x20000u, XBOX_CONTIG_SIZE + 0x21000u, true);
        /* The last mapped page is valid; a misaligned page there is not.
         * A rejected second page must leave the whole PRAM image intact. */
        bootstrap(apu, gp != 0, 0x10000u, 0x20000u,
                  2u * XBOX_CONTIG_SIZE - 4096u, true);
        bootstrap(apu, gp != 0, 0x10000u, 0x20000u,
                  2u * XBOX_CONTIG_SIZE - 4095u, false);
    }
    for (unsigned gp = 0; gp < 2; ++gp)
        for (unsigned fifo = 0; fifo < 2; ++fifo)
            for (unsigned table_bank = 0; table_bank < 2; ++table_bank)
                for (unsigned view = 0; view < 2; ++view)
                    check_dma(apu, gp != 0, fifo != 0, table_bank != 0, view != 0);
    mcpx_apu_dsp_finalize(apu);
    qemu_mutex_destroy(&apu->lock);
    free(apu); free(ram);
    printf("%u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
