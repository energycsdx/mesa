# Copyright © 2017 Intel Corporation
# Copyright © 2018 Alyssa Rosenzweig

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

files_panfrost = files(
  'pan_public.h',
  'pan_screen.c',
  'pan_screen.h',
  'pan_resource.c',
  'pan_resource.h',

  'midgard/midgard_compile.c',
  'midgard/mir.c',
  'midgard/midgard_print.c',
  'midgard/midgard_schedule.c',
  'midgard/midgard_emit.c',
  'midgard/midgard_ra.c',
  'midgard/midgard_ra_pipeline.c',
  'midgard/midgard_liveness.c',
  'midgard/midgard_ops.c',

  'midgard/nir_lower_blend.c',
  'midgard/cppwrap.cpp',
  'midgard/disassemble.c',

  'bifrost/disassemble.c',

  'pandecode/common.c',
  'pandecode/decode.c',

  'pan_context.c',
  'pan_afbc.c',
  'pan_blit.c',
  'pan_job.c',
  'pan_drm.c',
  'pan_allocate.c',
  'pan_assemble.c',
  'pan_format.c',
  'pan_blending.c',
  'pan_blend_shaders.c',
  'pan_pretty_print.c',
  'pan_fragment.c',
  'pan_invocation.c',
  'pan_instancing.c',
  'pan_scoreboard.c',
  'pan_sfbd.c',
  'pan_mfbd.c',
  'pan_tiler.c',
)

panfrost_includes = [
  inc_common,
  inc_gallium,
  inc_gallium_aux,
  inc_include,
  inc_src,
  inc_panfrost,
  include_directories('include'),
  include_directories('midgard'),
  include_directories('bifrost'),
]

compile_args_panfrost = [
  '-DGALLIUM_PANFROST',
  '-Wno-pointer-arith'
]

midgard_nir_algebraic_c = custom_target(
  'midgard_nir_algebraic.c',
  input : 'midgard/midgard_nir_algebraic.py',
  output : 'midgard_nir_algebraic.c',
  command : [
    prog_python, '@INPUT@',
    '-p', join_paths(meson.source_root(), 'src/compiler/nir/'),
  ],
  capture : true,
  depend_files : nir_algebraic_py,
)

libpanfrost = static_library(
  'panfrost',
  [files_panfrost, midgard_nir_algebraic_c],
  dependencies: [
    dep_thread,
    dep_libdrm,
    idep_nir
  ],
  include_directories : panfrost_includes,
  c_args : [c_vis_args, c_msvc_compat_args, compile_args_panfrost],
)

driver_panfrost = declare_dependency(
  compile_args : compile_args_panfrost,
  link_with : [libpanfrost, libpanfrostwinsys, libpanfrost_shared],
)

files_bifrost = files(
  'bifrost/disassemble.c',
  'bifrost/cmdline.c',
)

bifrost_compiler = executable(
  'bifrost_compiler',
  [files_bifrost],
  include_directories : [
    inc_common,
    inc_src,
    inc_include,
    inc_gallium,
    inc_gallium_aux,
    include_directories('bifrost')
    ],
  dependencies : [
    dep_thread,
    idep_nir
  ],
  link_with : [
    libgallium,
    libglsl_standalone,
    libmesa_util
  ],
  build_by_default : true
)

files_pandecode = files(
  'pandecode/cmdline.c',
  'pandecode/common.c',
  'pandecode/decode.c',

  'pan_pretty_print.c',

  'midgard/disassemble.c',
  'midgard/midgard_ops.c',
  'bifrost/disassemble.c',
)

pandecode = executable(
  'pandecode',
  files_pandecode,
  include_directories : panfrost_includes,
  dependencies : [
    dep_thread,
  ],
  link_with : [
    libmesa_util
  ],
  build_by_default : true
)
