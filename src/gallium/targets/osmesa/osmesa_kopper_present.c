/*
 * Zink+OSMesa unilateral present - Step 1: layer discovery probe.
 *
 * Runs inside the app process and locates the CAMetalLayer backing the key
 * window using only the Objective-C runtime (no link-time dependency on
 * UIKit/QuartzCore beyond libobjc, which OSMesa already links for Zink).
 * No behavior change yet: this only finds and logs.
 */

#include "osmesa_kopper_present.h"

#ifdef __APPLE__

#include <stdbool.h>
#include <string.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <dispatch/dispatch.h>
#include <stdio.h>

#include "util/u_debug.h"
#include "util/log.h"

struct find_layer_ctx {
   void *layer;
   char window_class[128];
   char layer_class[128];
};

static void
find_layer_on_main(void *arg)
{
   struct find_layer_ctx *ctx = arg;
   ctx->layer = NULL;
   ctx->window_class[0] = '\0';
   ctx->layer_class[0] = '\0';

   Class UIApplicationClass = objc_getClass("UIApplication");
   if (!UIApplicationClass)
      return;
   SEL sharedAppSel = sel_registerName("sharedApplication");
   id (*sharedAppFn)(id, SEL) = (void *)objc_msgSend;
   id app = sharedAppFn((id)UIApplicationClass, sharedAppSel);
   if (!app)
      return;

   SEL keyWindowSel = sel_registerName("keyWindow");
   id (*keyWindowFn)(id, SEL) = (void *)objc_msgSend;
   id window = keyWindowFn(app, keyWindowSel);
   if (!window)
      return;

   snprintf(ctx->window_class, sizeof(ctx->window_class), "%s",
            object_getClassName(window));

   SEL layerSel = sel_registerName("layer");
   id (*layerFn)(id, SEL) = (void *)objc_msgSend;
   id layer = layerFn(window, layerSel);
   if (!layer)
      return;

   snprintf(ctx->layer_class, sizeof(ctx->layer_class), "%s",
            object_getClassName(layer));

   /* Accept only real Metal layers for now. */
   if (strcmp(ctx->layer_class, "CAMetalLayer") == 0)
      ctx->layer = layer;
}

void *
osmesa_kopper_find_layer(void)
{
   struct find_layer_ctx ctx;

   /* UIKit must be touched on the main thread. */
   Class NSThreadClass = objc_getClass("NSThread");
   bool on_main = false;
   if (NSThreadClass) {
      SEL isMainSel = sel_registerName("isMainThread");
      BOOL (*isMainFn)(id, SEL) = (void *)objc_msgSend;
      id cur = nil;
      /* +currentThread */
      SEL curSel = sel_registerName("currentThread");
      id (*curFn)(id, SEL) = (void *)objc_msgSend;
      cur = curFn((id)NSThreadClass, curSel);
      if (cur)
         on_main = isMainFn(cur, isMainSel);
   }

   if (on_main) {
      find_layer_on_main(&ctx);
   } else {
      dispatch_sync_f(dispatch_get_main_queue(), &ctx, find_layer_on_main);
   }

   /* debug_printf is compiled out in release builds - use mesa_loge so the
    * probe result is visible in device logs. */
   mesa_loge("OSMesa Kopper: layer probe window=%s layer=%s result=%p",
             ctx.window_class[0] ? ctx.window_class : "(none)",
             ctx.layer_class[0] ? ctx.layer_class : "(none)",
             ctx.layer);
   return ctx.layer;
}

#else /* !__APPLE__ */

void *
osmesa_kopper_find_layer(void)
{
   return NULL;
}

#endif
