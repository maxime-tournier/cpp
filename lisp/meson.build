
project('lisp', 'cpp', default_options : ['cpp_std=c++11'])

sources = ['main.cpp',
           'symbol.cpp',
           'sexpr.cpp',
           'parse.cpp',
           'syntax.cpp',           
           'eval.cpp',
           'vm.cpp',
           'tool.cpp',
           'codegen.cpp',
           'jit.cpp']

compiler = meson.get_compiler('cpp')

readline = compiler.find_library('readline', required: true)
stdcpp = compiler.find_library('stdc++', required: true)
boost = dependency('boost', modules : ['program_options'] )

if get_option('buildtype') == 'release' or get_option('buildtype') == 'debugoptimized'
  add_project_arguments('-DNDEBUG', language: 'cpp')
endif


executable('lisp', sources, 
		   dependencies : [readline, stdcpp, boost],
		   install: true,
		   install_dir: meson.source_root() )
