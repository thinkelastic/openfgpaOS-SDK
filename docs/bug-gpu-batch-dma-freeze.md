# Open: GPU hangs after a few seconds of CMD_DRAW_SPANS_BATCH traffic

## Status

**Open.** Reproducible on Pocket hardware with PocketDukeNukem-SDK
duke3d core (post-commit 3531b2a per-span colormap_id wire format).
Hangs within a few seconds of entering an in-game level; the symptom
predates the colormap_id change but landed faster after it (bigger
batches → more DMA traffic per frame).

A workaround is in place in PocketDukeNukem-SDK's vendored
`src/sdk/include/of_gpu.h`: `of_gpu_draw_spans_batch` forces the
per-span MMIO fallback via a `1 ||` guard at the
`if (_gpu_batch_buf == NULL)` site (search `TEMP DIAG`). Frame rate
is essentially unchanged (~34 fps either way) because the DMA path's
`while (GPU_STATUS & GPU_STATUS_DMA_BUSY)` was eating most of the
theoretical batching win at this workload.

## Symptom

Last observed trap before the workaround landed:

```
mcause = 0x00000002    (illegal instruction)
mepc   = 0x1032be44    (unmapped SDRAM)
mtval  = 0x0786471f    (garbage instruction word)
```

i.e. control flow jumped to garbage in unmapped SDRAM. No
`__builtin_trap` from `of_gpu_wait`'s 50M-spin watchdog ever fired,
so the process was still making forward progress when it derailed —
consistent with a stack/code corruption rather than a pure GPU spin.

## Workload at time of freeze

- Caller: Duke3D BUILD wall/sprite/floor renderers via
  `d3d_gpu_submit_span` → `d3d_gpu_flush_batch` → `of_gpu_draw_spans_batch`.
- Batch size: ~120 spans/batch (right at `OF_GPU_BATCH_MAX_SPANS` = 128).
- Batches per frame: ~14 (was ~30 before per-span colormap_id; now
  almost entirely buffer-full, no state-driven flushes).
- Spans per frame: ~1850.
- Each batch payload: 15 × ~120 = ~1800 words (7200 bytes), under the
  16 KB scratch buffer at `caps->sdram_base + OF_GPU_BATCH_BUF_AXI_OFFSET`.

## Bisection signal

In `src/sdk/include/of_gpu.h`, change the line at the top of
`of_gpu_draw_spans_batch`:

```c
if (1 || _gpu_batch_buf == NULL) {     /* freeze gone */
if (   _gpu_batch_buf == NULL) {       /* freeze returns within seconds */
```

Same span encoder, same caller, same scratch buffer — the only thing
that changes is whether the helper emits N standalone `CMD_DRAW_SPAN`
commands via per-word MMIO, or builds the batch in SDRAM and uses
`CMD_DRAW_SPANS_BATCH` + `GPU_DMA_KICK`. So the bug lives in:

1. The DMA path itself (m_rd_* AXI master pulling from SDRAM into
   the ring BRAM), or
2. The header-MMIO + DMA atomic publish handoff (CPU writes the
   `CMD_DRAW_SPANS_BATCH` header word into the ring BRAM via
   `GPU_RING_DATA`, then DMA streams the payload and lifts ring_wrptr
   to expose the whole command), or
3. The `port-A mux picks DMA when both fire same cycle` path the
   helper's existing comment block flags as the reason for the
   `wait DMA_BUSY` at the end of every batch.

## Suspects worth chasing first

- **port-A mux**: helper claims the trailing `wait DMA_BUSY` covers
  CPU↔DMA contention on ring BRAM port-A. Verify in RTL that the
  next `_gpu_ring_write` after `wait DMA_BUSY` returns truly cannot
  collide with a still-in-flight DMA write — including the case where
  the CPU's MMIO write of the batch header word races the DMA's
  first beat (header is written before `GPU_DMA_KICK` but the GPU
  decoder might fetch it before DMA publishes wrptr).

- **fence-wait race**: kernel agent's prior TEMP TRACE was already
  chasing a fence-wait race in this region (per
  PocketDukeNukem-SDK conversation summary). The mcause=2 unmapped
  PC argues for memory corruption, not a stuck spin — but a
  fence-reached-but-data-stale race could let the rasteriser walk
  off into unmapped territory if a span's `fb_addr` arrived as
  stale ring bytes.

- **m_rd_* AXI bounds**: confirm the DMA respects exactly
  `payload_words` (1920 max) and never overruns into adjacent SDRAM
  (heap, stack, .text). The scratch buffer is 16 KB at offset
  `0x140000`, immediately above the 256 KB palookup window —
  if the DMA wraps wrong, it could trash palookup bytes.

## Reproducing

1. Clone PocketDukeNukem-SDK at HEAD.
2. In `src/sdk/include/of_gpu.h`, replace `1 || _gpu_batch_buf == NULL`
   with `_gpu_batch_buf == NULL` (re-enable DMA path).
3. `make build CORE=duke3d`, deploy to Pocket.
4. Start a new game on the first level (E1L1).
5. Hangs within ~5–10 seconds of moving / shooting.

## Diagnostic helpers already in tree

- `of_gpu_draw_spans_batch` has a TEMP TRACE that prints the first 30
  batches' `rdptr / sdk_wrptr / ring_wrptr / free / dma_busy` at kick
  time. Useful for spotting a wrptr that drifts apart from rdptr in
  ways that look like a missed DMA-publish.
- `of_gpu_wait` has a 50M-iteration watchdog that traps via
  `__builtin_trap` (mcause=2 / mtval=0xc0001073 from the `unimp`).
  Distinguishes "GPU is stuck" from "CPU walked off a cliff".
