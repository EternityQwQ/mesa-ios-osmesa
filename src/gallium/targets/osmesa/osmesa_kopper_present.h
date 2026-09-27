/*
 * Zink+OSMesa unilateral present: find the app's Metal layer and present
 * rendered frames straight to it, bypassing the CPU readback path.
 *
 * Staged rollout:
 *   Step 1 (this file): layer discovery probe only - no behavior change.
 *   Step 2: VkSurface + swapchain create/recreate around the layer.
 *   Step 3: blit OSMesa result into swapchain image + present on flush.
 *
 * Everything here is Apple-only. Other platforms compile to no-ops.
 */

#ifndef OSMESA_KOPPER_PRESENT_H
#define OSMESA_KOPPER_PRESENT_H

/* The app hands its CAMetalLayer over explicitly (opaque pointer, mesa
 * never retains/releases it - ownership stays with the app).
 * Exported with default visibility so the app can dlsym it.
 * May be called before first use; passing NULL clears it. */
void
osmesa_kopper_set_layer(void *metal_layer);

/* Returns the explicit layer if set, else probes the key window.
 * Safe to call repeatedly; logs what it finds. */
void *
osmesa_kopper_find_layer(void);

#endif
