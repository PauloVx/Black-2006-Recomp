# Screen-space reflections

BLACK draws its shiny surfaces (mostly the first-person weapons, plus some
windows and glass) with a 128 x 128 sphere-mapped environment texture. The
GPU renderer can replace that map with reflections of the scene on screen.

Choose **Reflections** on the launcher's **Graphics** tab or the in-game
**Video Settings > Graphics** page:

| Value    | Effect                                                        |
|----------|---------------------------------------------------------------|
| Original | The game's own environment maps, unchanged.                   |
| Low      | Screen-space reflections, 24 march steps.                     |
| Medium   | 40 steps.                                                     |
| High     | 64 steps (default).                                           |
| Ultra    | 96 steps.                                                     |

The setting is stored as `reflections=original|low|medium|high|ultra` in
`local/video.ini`. `RECOMP_REFLECTIONS` pins it for one run, like the other
video environment variables. It needs the GPU renderer.

## How it works

1. **Finding the environment-mapped draws.** BLACK has no cube maps. Its vertex
   programs compute a reflection vector and write a sphere-map coordinate with
   `mad oT, r.xz, c.y, c.y` (`c.y = 0.5`) from a normal read from `v2`.
   `vsh_env_stages` (in `kelvin_gpu_vsh.inc`) finds the texture stages that
   such a program feeds.
2. **Splitting what the map adds.** For those draws the renderer builds a
   variant of the combiner pixel shader. It evaluates the combiners twice: once
   with the environment texel at 1 and once at 0. The difference is how much
   the map contributes (the reflection weight). The variant writes three extra
   targets: the weight, what the original map added, and the view depth plus a
   normal taken from the screen-space derivatives of the view position.
3. **The pass.** After ambient occlusion and before the HUD,
   `kg_ssr_pass` (`kelvin_gpu_ssr.inc`) reflects the view ray about that
   normal. It marches it through the depth buffer, refines a hit with a
   binary search, and blurs farther hits through the scene's mip chain. The
   map's contribution is then swapped for `weight x scene colour`.
   - A ray that leaves the screen, or that hits nothing, uses a blurred copy of
     the screen looked up like a sphere map. This keeps the colours of the
     scene instead of the baked map.
   - The environment maps assume a lit scene. Night scenes are much darker, so
     the reflected colour is brightened toward that level, by at most 4x. It is
     never darkened.

The overall brightness of the shiny surfaces matches the original (mean
luminance 18.3 vs 18.4 at the start of mission 5). The blocky 128-texel
highlights are replaced by the scene's own colours and shapes.

## Diagnostics

| Variable | Use |
|----------|-----|
| `RECOMP_KGPU_SSR_DUMP=PREFIX[,N]` | Writes `PREFIX_N_before/after/weight.bmp` at the Nth pass and every 300th after it. |
| `RECOMP_KGPU_SSR_ENV` | Share (0..1) of a missed ray that the screen environment takes from the original map. Default 1. |
| `RECOMP_KGPU_SSR_STRENGTH` | Most a dark scene's reflection is brightened (1..8). Default 4. |
| `RECOMP_KGPU_POST_TRACE=1` | Also logs `[KSSR]` frame and draw counts. |
| `RECOMP_KELVIN_TRACE=first[-last]` | On the GPU renderer too: combiner stages, vertex program and textures. `RECOMP_KELVIN_TRACE_TEX=PREFIX` also dumps the textures. |
