# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_bcm_host%': 0,
    'internal_ozone_platform_deps': [
      'ozone_platform_egl',
    ],
    'internal_ozone_platforms': [
      'egl'
    ],
  },
  'targets': [
    {
      'target_name': 'ozone_platform_egl',
      'type': 'static_library',
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../events/events.gyp:events',
        '../events/ozone/events_ozone.gyp:events_ozone_evdev',
        '../gfx/gfx.gyp:gfx',
      ],
      'sources': [
        'client_native_pixmap_factory_egl.cc',
        'client_native_pixmap_factory_egl.h',
        'ozone_platform_egl.cc',
        'ozone_platform_egl.h',
        'egl_surface_factory.cc',
        'egl_surface_factory.h',
        'ozone_platform_egl.h',
        'ozone_platform_egl.cc',
        'egl_wrapper.cc',
        'egl_wrapper.h',
        'egl_window.cc',
        'egl_window.h',
      ],
      'link_settings': {
            'libraries': [
              '-lEGL',
              '-lGLESv2',
            ],
      },
      'conditions': [
          ['<(use_bcm_host) == 1', {
              'defines': [
                  'EGL_API_BRCM',
              ],
              'cflags': [
                  '<!@(pkg-config --cflags bcm_host)',
              ],
              'link_settings': {
                  'ldflags': [
                      '<!@(pkg-config --libs-only-L --libs-only-other bcm_host)',
                  ],
                  'libraries': [
                      '<!@(pkg-config --libs-only-l bcm_host)',
                  ],
              },
          }],
      ],
    },
  ],
}
