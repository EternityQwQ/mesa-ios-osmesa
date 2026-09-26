# mesa-ios-osmesa

Mesa 源码（`mesa-main.zip`）+ iOS 真机构建流水线。

目标：只用 GitHub Actions（`macos-15`）构建适用于 iOS 真机（arm64，iPhoneOS SDK）的 dylib，不在本地（非 macOS）构建。

## 方案

- **OSMesa 离屏渲染（CPU，softpipe，无需 GPU）**：`libOSMesa.8.dylib`
  - 适用于后台 / 无窗口 / 缩略图 / 测试等离屏场景。
  - API：`include/GL/osmesa.h`，`OSMesaCreateContext` / `OSMesaMakeCurrent`。
- **Zink（GPU，Metal via MoltenVK）**：与 OSMesa 同一次构建产出（`gallium-drivers=zink,softpipe`）
  - Zink 是 Gallium-on-Vulkan 驱动，iOS 上 Vulkan 由 MoltenVK 提供。
  - 构建时用 `brew install molten-vk` 的头文件；运行时 iOS App 必须捆绑 MoltenVK XCFramework 并提供 `libvulkan`/`MoltenVK`（Zink 运行时 `dlopen(@rpath/...)`）。

## 工作流

`.github/workflows/build-ios-osmesa.yml`

- `osmesa-ios-arm64`：`softpipe` + `osmesa=true`，产物 `libOSMesa.8.dylib`（已验证 18MB arm64）。
- `zink-ios-arm64`：`zink,softpipe` + `osmesa=true` + `moltenvk-dir=$(brew --prefix molten-vk)`，产物 `dist/*.dylib`（OSMesa + 含 Zink 的 gallium dylib）+ meson 日志。
- 交叉编译：`arm64-apple-ios14.0` + `xcrun --sdk iphoneos`，`macos-15` runner，`meson>=1.4`，`bison>=3`，`flex`，`pkg-config`。
- 触发：`push main` / `PR` / `workflow_dispatch`。产物保留 30 天。

本地（Linux/Termux）只做解压/推送/改 YAML，不运行 `meson setup/compile`。

## 使用

1. Push 到 `main` 后去 Actions 下载：
   - `libOSMesa.8.dylib`（纯离屏）
   - `mesa-ios-arm64-zink-osmesa`（Zink+OSMesa 全套）
2. Xcode 引入：头文件 `include/GL/osmesa.h` + `dylib`，`LC_BUILD_VERSION` 为 iOS arm64。
3. OSMesa 最小调用：
   ```c
   #include "GL/osmesa.h"
   OSMesaContext ctx = OSMesaCreateContext(OSMESA_RGBA, NULL);
   void *buf = malloc(w*h*4);
   OSMesaMakeCurrent(ctx, buf, GL_UNSIGNED_BYTE, w, h);
   OSMesaPixelStore(OSMESA_Y_UP, 0);
   // gl* calls...
   glFinish();
   OSMesaDestroyContext(ctx);
   ```
   默认走 softpipe CPU 离屏；设环境变量 `GALLIUM_DRIVER=zink` 切到
   Zink+OSMesa（GPU 经 MoltenVK/Metal 离屏渲染并回读，输出仍是内存
   像素，无需窗口；设备上缺 Vulkan 时自动回落 softpipe）。
4. Zink on iOS：App 先加载 MoltenVK（`MoltenVK.xcframework/ios-arm64`），再加载 Mesa dylib，`GALLIUM_DRIVER=zink`（或默认 pipe-loader 选 zink），Vulkan 层走 Metal。

## 源码来源

- `mesa-main.zip`（VERSION：26.3.0-devel），解压为 `mesa-main/`。
- 相对上游补回（上游已移除）：`src/gallium/frontends/osmesa/`，`src/gallium/targets/osmesa/`，`include/GL/osmesa.h`。
- 禁止改动：`src/asahi/`，`src/gallium/drivers/asahi/`。
