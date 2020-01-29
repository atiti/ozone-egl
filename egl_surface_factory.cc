// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "egl_surface_factory.h"
#include "egl_ozone_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/public/surface_ozone_egl.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "base/logging.h"
#include "ui/ozone/common/egl_util.h"

#include <fcntl.h>  /* For O_RDWR */
#include <unistd.h> /* For open(), creat() */
#include <linux/fb.h>
#include <sys/ioctl.h>

// GPU tiled value for supertiled-32bit is taken from kernel.
// If not defined for a given platform, we define it as 0 (it's
// the case of Airtame DG1 for example)
#include <linux/ipu.h>
#ifndef IPU_PIX_FMT_GPU32_SRT
#define IPU_PIX_FMT_GPU32_SRT 0
#endif

#define OZONE_EGL_WINDOW_WIDTH 1024
#define OZONE_EGL_WINDOW_HEIGTH 768

namespace {
// This is a fix for trashed HDMI output when Chromium execution ends
// (such as after reboot command was issued) on i.MX6 with Vivante GC2000
// GPU. Problem is that EGL attempts to restore framebuffer settings to
// their previous (before Chromium started) values, but won't clear
// framebuffer content. Often we end up having supertiled content
// (because that is what GC2000 renders) and settings as if for ordinary
// bitmaps (because that was set before Chromium started). HDMI tries to
// display supertiled content as it if was bitmap, and that creates
// "scrambled" image. To avoid this problem, we set 32-bit supertiling
// before EGL starts.
bool SetGpuTileFormat(const uint32_t iGPUTileFormat) {
    // Open framebuffer
    int fb_handle = open("/dev/fb0", O_RDWR);
    if (-1 == fb_handle) {
        LOG(ERROR) << "Failed to open framebuffer";
        return false;
    }

    // Get variable info
    fb_var_screeninfo vinfo = {};
    if (-1 == ioctl(fb_handle, FBIOGET_VSCREENINFO, &vinfo)) {
        LOG(ERROR) << "Failed to get the variable framebuffer info";
        close(fb_handle);
        return false;
    }

    // Check if current value is equal to iGPUTileFormat
    if (iGPUTileFormat != vinfo.nonstd) {
        // Nope, we have to reset
        vinfo.nonstd = iGPUTileFormat;
        if (-1 == ioctl(fb_handle, FBIOPUT_VSCREENINFO, &vinfo)) {
            LOG(ERROR) << "Resetting vinfo.nonstd failed";
            close(fb_handle);
            return false;
        }
    }
    // OK, that is all
    close(fb_handle);
    return true;
}

std::string GetPlatformName() {
    auto env_platform = std::getenv("AIRTAME_PLATFORM");
    return (env_platform) ? env_platform : "";
}
}

namespace ui {

// Specific implementation of SurfaceOzoneEGL in the context of CreateEGLSurfaceForWidget()
// as called from the GPU process (not used when in software-drawing mode)
class OzoneEgl : public ui::SurfaceOzoneEGL {
 public:
  OzoneEgl(gfx::AcceleratedWidget window_id) { native_window_ = window_id; }
  ~OzoneEgl() override { native_window_ = 0; }

  intptr_t GetNativeWindow() override { return native_window_; }

  bool OnSwapBuffers() override { return true; }

  void OnSwapBuffersAsync(const SwapCompletionCallback& callback) override {}

  bool ResizeNativeWindow(const gfx::Size& viewport_size) override {
    return true;
  }

  scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() override {
    return scoped_ptr<gfx::VSyncProvider>();
  }

  // Returns the EGL configuration to use for this surface. The default EGL
  // configuration will be used if this returns nullptr.
  void* /* EGLConfig */ GetEGLSurfaceConfig(
      const EglConfigCallbacks& egl) override {
    return nullptr;
  }

 private:
  intptr_t native_window_;
};

SurfaceFactoryEgl::SurfaceFactoryEgl() {
    if (GetPlatformName() == "DG2") {
        // set GPU tile format to super-tiled 32bit before
        // calling fbCreateWindow which sets nonstd flag for
        // /dev/fb0 with current fb_var_screeninfo
        SetGpuTileFormat(IPU_PIX_FMT_GPU32_SRT);
    }

    CreateNativeWindow();
}

SurfaceFactoryEgl::~SurfaceFactoryEgl() {
    if (GetPlatformName() == "DG2") {
        // set GPU tile format to super-tiled 32bit to
        // make sure to have a proper default value at
        // the following startup
        SetGpuTileFormat(IPU_PIX_FMT_GPU32_SRT);
    }
}

// TODO: This should be called only in "accelerated drawing" mode. Calling in
// "software" mode has no effect.
bool SurfaceFactoryEgl::CreateNativeWindow() {
  int window_width=0;
  int window_height=0;

  native_display_ = (NativeDisplayType)fbGetDisplayByIndex(0);
  fbGetDisplayGeometry(native_display_,&window_width,&window_height);
  native_window_ = fbCreateWindow(native_display_, 0, 0, window_width, window_height);

  return true;
}


intptr_t SurfaceFactoryEgl::GetNativeDisplay() {
  return (intptr_t)native_display_;
}

intptr_t SurfaceFactoryEgl::GetNativeWindow() {
  return (intptr_t)native_window_;
}

scoped_ptr<ui::SurfaceOzoneEGL> SurfaceFactoryEgl::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  return make_scoped_ptr<ui::SurfaceOzoneEGL>(new OzoneEgl(widget));
}

bool SurfaceFactoryEgl::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return LoadDefaultEGLGLES2Bindings(add_gl_library, set_gl_get_proc_address);
}

scoped_ptr<ui::SurfaceOzoneCanvas> SurfaceFactoryEgl::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  return make_scoped_ptr<SurfaceOzoneCanvas>(new EglOzoneCanvas());
}

}  // namespace ui
