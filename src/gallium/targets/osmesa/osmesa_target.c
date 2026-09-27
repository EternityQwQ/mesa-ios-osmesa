/*
 * Copyright (c) 2013  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GALLIUM_ZINK
#include "zink/zink_public.h"
#endif

#include "target-helpers/inline_sw_helper.h"
#include "target-helpers/inline_debug_helper.h"

#include "sw/null/null_sw_winsys.h"

#include "osmesa_kopper_present.h"

#ifdef __APPLE__
/* iOS apps have no visible stderr: record which screen OSMesa ended up
 * with (zinkfail.txt in the process working directory, same file the
 * Zink fail stages go to). Best effort. */
static void
osmesa_note(const char *what)
{
   const char *path = getenv("ZINK_FAIL_LOG");
   FILE *f = fopen(path && path[0] ? path : "zinkfail.txt", "a");
   if (f) {
      fprintf(f, "%s\n", what);
      fclose(f);
   }
}
#else
static inline void
osmesa_note(const char *what)
{
   (void)what;
}
#endif


struct pipe_screen *
osmesa_create_screen(void);


struct pipe_screen *
osmesa_create_screen(void)
{
   struct sw_winsys *winsys;
   struct pipe_screen *screen;

   /* We use a null software winsys since we always just render to ordinary
    * driver resources.
    */
   winsys = null_sw_create();
   if (!winsys)
      return NULL;

#ifdef GALLIUM_ZINK
   /* Zink+OSMesa offscreen rendering: GPU through Vulkan (MoltenVK/Metal
    * on iOS) is mandatory - no software fallback. Output stays offscreen:
    * the frontend reads pixels back via resource transfers, no window or
    * swapchain is ever needed. A NULL return here means Zink/MoltenVK is
    * unusable on this device (see zinkfail.txt stages).
    */
   screen = zink_create_screen(winsys, NULL);
   if (screen) {
      osmesa_note("OK:zink");
      /* Step 1 probe: locate the app Metal layer (logs only). */
      (void)osmesa_kopper_find_layer();
      return debug_screen_wrap(screen);
   }
   debug_printf("OSMesa: Zink unavailable, no software fallback\n");
   osmesa_note("FAIL:zink-screen");
   winsys->destroy(winsys);
   return NULL;
#else
   /* Create llvmpipe or softpipe screen */
   screen = sw_screen_create(winsys);
   if (!screen) {
      winsys->destroy(winsys);
      return NULL;
   }
   osmesa_note("OK:softpipe");

   /* Inject optional trace, debug, etc. wrappers */
   return debug_screen_wrap(screen);
#endif
}
