# Copyright © 2017 Intel Corporation
# Copyright (C) 2026 EternityQwQ (iOS portability changes)
# SPDX-License-Identifier: MIT

osmesa_link_args = []
osmesa_link_depends = []

if with_ld_version_script
  osmesa_link_args += [
    '-Wl,--version-script', join_paths(meson.current_source_dir(), 'osmesa.sym')
  ]
  osmesa_link_depends += files('osmesa.sym')
endif

osmesa_def = custom_target(
  'osmesa.def',
  input: 'osmesa.def.in',
  output : 'osmesa.def',
  command : gen_vs_module_defs_normal_command,
)

osmesa_extra_drivers = []
if with_gallium_zink
  # Zink+OSMesa offscreen: link Zink so GALLIUM_DRIVER=zink can select
  # GPU rendering (MoltenVK/Metal on iOS) with software fallback.
  osmesa_extra_drivers += driver_zink
  # Zink reads DRI options at screen creation; the xmlconfig stub lib
  # provides those symbols without needing expat.
  osmesa_extra_drivers += idep_xmlconfig
endif
if host_machine.system() == 'darwin' and with_gallium_zink
  # Zink's Metal code is Objective-C and needs the ObjC runtime at link.
  osmesa_link_args += '-lobjc'
endif

libosmesa = shared_library(
  osmesa_lib_name,
  'osmesa_target.c',
  gnu_symbol_visibility : 'hidden',
  link_args : [ld_args_gc_sections, osmesa_link_args],
  vs_module_defs : osmesa_def,
  include_directories : [
    inc_include, inc_src, inc_gallium, inc_gallium_aux, inc_gallium_winsys,
    inc_gallium_drivers, inc_mesa, inc_frontends,
  ],
  link_depends : osmesa_link_depends,
  link_whole : [libosmesa_st, libglapi_bridge],
  link_with : [libmesa, libgallium, libws_null, libglapi],
  dependencies : [dep_thread, dep_clock, dep_unwind, driver_swrast, idep_mesautil] + osmesa_extra_drivers,
  name_prefix : host_machine.system() == 'windows' ? '' : [],
  soversion : host_machine.system() == 'windows' ? '' : '8',
  version : '8.0.0',
  darwin_versions : '9.0.0',
  install : true,
)

pkg.generate(
  name : 'osmesa',
  description : 'Mesa Off-screen Rendering Library',
  version : '8.0.0',
  libraries : libosmesa,
  libraries_private : gl_priv_libs,
)
