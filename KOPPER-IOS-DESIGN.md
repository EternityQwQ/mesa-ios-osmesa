# Zink Kopper 上屏iOS 详细设计（kopper-ios 分支）

目标：iOS 真机经 **OpenGL → Zink → MoltenVK → Metal 直显**，不再走 OSMesa 离屏回读。
现状：OSMesa 离屏链已通（`main` 分支，GL 4.1，`libOSMesa.8.dylib`）；Kopper 在 Apple 侧空白。
原则：OSMesa 与 Kopper 共存（App 渲染器选项切换），mesa 侧改动 Apple/显式隔离，桌面构建零影响。

## 1. 总体数据流

```
MC/LWJGL (GL/EGL API)
  → EGL[DRI2 + platform_ios] -- eglCreateWindowSurface(native_window=CAMetalLayer*)
  → DRI frontend (kopper path, __DRI_KOPPER_LOADER)
  → Zink kopper (swapchain, present)
  → MoltenVK (VK_EXT_metal_surface) → CAMetalLayer → 屏幕
```

对比 OSMesa 链：省掉每帧 GPU→CPU 回读 + `glFinish` 强同步；vsync 走 swapchain present modes。

## 2. 已有可复用的东西（勿重造）

- `src/egl/drivers/dri2/egl_dri2.c`：已有 `kopper_pbuffer_loader_extension` + `dri2_detect_swrast_kopper`（Zink 自动切 kopper），EGL↔DRI 框架现成。
- `src/gallium/frontends/dri/{kopper.c,dri_drawable.c}`：kopper screen/drawable 通用逻辑现成；`screen->kopper_loader->SetSurfaceCreateInfo(draw, &info)` 是唯一的平台相关注入点。
- `platform_wayland.c:2459 kopperSetSurfaceCreateInfo`：iOS 版照抄结构（填 `VkMetalSurfaceCreateInfoEXT` 进 `out->bos`）。
- App 侧：Metallum surface view（`CAMetalLayer` 来源）、`egl_bridge`（已有 LWJGL EGL 通路）、`vk_bridge`（MoltenVK 加载经验）。

## 3. 接口定义（三方契约，冻结后不得随意改）

### 3.1 App → Mesa（EGL 层）

- 新增 `EGL_PLATFORM_IOS_MESA`（私有 enum，取值避开 Khronos 已分配段，配 `EGL_EXT_platform_ios_mesa` 扩展字符串上报）。
- `eglGetPlatformDisplay(EGL_PLATFORM_IOS_MESA, NULL, NULL)`；`eglCreateWindowSurface(dpy, config, (EGLNativeWindowType)CAMetalLayer*, NULL)`，`native_window` 即 `CAMetalLayer*`（main 线程创建，常驻）。
- App 保证：layer 常驻（surface 销毁前不释放）、尺寸/朝向变化只改 `layer.bounds`+`drawableSize`（Mesa 侧每帧经 `GetDrawableInfo` 重读，不缓存宽高）。

### 3.2 Mesa EGL platform_ios → DRI/kopper（`__DRIkopperLoaderExtension`）

- `SetSurfaceCreateInfo(draw, out)`：`VkMetalSurfaceCreateInfoEXT{ sType, pNext=NULL, flags=0, pLayer=(CAMetalLayer*)native_window }` 写入 `out->bos`；`out->present_opaque`、`initial_swap_interval`、`compression`、`has_alpha` 按 EGL surface 属性填。
- `GetDrawableInfo(draw, w, h)`：每次从 `CAMetalLayer.drawableSize`（×`contentsScale`）重读，回填 w/h（旋转/分屏即时生效）。
- 定长断言：`static_assert(sizeof(kopper_vk_surface_create_storage) >= sizeof(VkMetalSurfaceCreateInfoEXT))`（flags+单指针，装得下，仿 wayland 写法）。

### 3.3 DRI/kopper → MoltenVK

- instance 扩展新增 `VK_EXT_metal_surface`（Apple 侧，仿 `portability_enumeration` 的 Darwin 条件写法）。
- `kopper_CreateSurface` 新增 `VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT` 分支：`vkCreateMetalSurfaceEXT`；其后现有的 surface-support/present-modes 查询与 swapchain 创建逻辑复用。
- present 模式：以 MoltenVK 上报为准（FIFO 必有；MAILBOX/IMMEDIATE 按上报取舍，vsync interval 映射到现有 `present_mode` 逻辑）。

## 4. Mesa 侧改动清单（kopper-ios 分支）

| # | 文件 | 改动 | 量级 |
|---|---|---|---|
| 1 | `src/gallium/drivers/zink/zink_instance.py` | instance 扩展表加 `VK_EXT_metal_surface`（Darwin 条件） | 小 |
| 2 | `src/gallium/drivers/zink/zink_kopper.c` | `kopper_CreateSurface` 加 METAL 分支 + 定长断言 | 小 |
| 3 | `src/egl/drivers/dri2/platform_ios.[ch]` | 新建：display 初始化、surface 创建（存 `CAMetalLayer*`）、上面两个 loader 回调、loader extensions 数组 | 大（仿 `platform_wayland.c` 精简，wayland 特有粘贴/代理逻辑不要） |
| 4 | `src/egl/drivers/dri2/egl_dri2.c` + `meson.build` | 接入 platform_ios（probe/构造/销毁 nearest-wayland 写法） | 中 |
| 5 | `src/egl/main/*`, `eglplatform.h` | `EGL_PLATFORM_IOS_MESA` enum + 扩展字符串 | 小 |
| 6 | `meson.options` + `meson.build` | `platforms` 加 `'ios'` 选项 + `with_platform_ios`；darwin 的 auto 保持 `x11,macos` 不变，iOS 构建一律显式 `-Dplatforms=ios` | 中（动公共文件，评审重点） |
| 7 | `.github/workflows/build-ios-osmesa.yml` | 新增 kopper job（或扩展 zink job）：`-Dplatforms=ios -Degl=enabled -Dglx=disabled`，产物 `libEGL.dylib + libGLESv2.dylib`（或对应 megadriver） | 中 |
| 8 | `frontends/dri` | 预期零改动（kopper 路径通用）；实现期复核 `is_window` 判定（`bos.sType != 0` 即窗口，Metal 满足） | 复核 |

## 5. App 侧改动（Amethyst 仓库，他们的人做，不在本分支）

- `egl_bridge`：Zink 选项改走 EGL（`eglGetPlatformDisplay` + `eglCreateWindowSurface(layer)`），替代现在的 OSMesa bridge；present 走 `eglSwapBuffers`（kopper 内 swapchain present），删掉读回上屏。
- `CAMetalLayer` 由 Metallum view 提供（已存在），`drawableSize` 跟随视图。
- LWJGL 走 EGL binding（ANGLE 路径已有先例）。
- 保留 OSMesa 选项作回退（我们 dylib 双路共存）。

## 6. 验证里程碑（每步真机可判定）

- M1：mesa 侧编译过（CI），`vkCreateMetalSurfaceEXT` 符号在，instance 扩展上报含 metal_surface。
- M2：EGL 初始化 + 建 window surface 成功（log 打点，不崩）。
- M3：swapchain 建链 + 第一帧 present（屏幕有画面，哪怕黑三角）。
- M4：MC 菜单/全景正常（cube 路径复用 OSMesa 期全部修复）。
- M5：vsync/旋转/分屏/切后台不崩；帧率对比 OSMesa（预期主要赢在省掉回读）。

## 7. 风险

- R1：MoltenVK swapchain present 在 A11 的 present-mode/层数限制 → M3 验证，退路：FIFO + 单层 swapchain（kopper 本来就这么干）。
- R2：`platforms=ios` 动公共 meson 文件 → 与上游 rebase 冲突面，改动集中注释标记 `iOS Kopper`。
- R3：App 侧 EGL 切换工作量不在本分支，双方按 §3 接口对接，联调以 M2/M3 为界。
- 非目标：macOS（`__APPLE__` 通用代码保持 macOS 可编，但验证只做 iOS 真机）；X11/GLX 不动。
