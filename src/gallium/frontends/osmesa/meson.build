# Copyright © 2017-2018 Intel Corporation
# SPDX-License-Identifier: MIT

libosmesa_st = static_library(
  'osmesa_st',
  'osmesa.c',
  include_directories : [
    inc_include, inc_src, inc_gallium, inc_gallium_aux, inc_gallium_winsys,
    inc_mesa, inc_frontends,
  ],
  dependencies : [idep_mesautil],
)
