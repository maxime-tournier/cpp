
project('slip', 'cpp', default_options : ['cpp_std=c++11'])

sources = ['main.cpp',
           'symbol.cpp',
           'sexpr.cpp',
           'parse.cpp',
           'ast.cpp',           
           'vm.cpp',
           'tool.cpp',
           'codegen.cpp',
           'jit.cpp',
           'types.cpp' ]

compiler = meson.get_compiler('cpp')

readline = compiler.find_library('readline', required: true)

boost = dependency('boost', modules : ['program_options'], required: true)
# boost = compiler.find_library ('boost_program_options', required: true)

std = compiler.find_library ('stdc++', required: true)

if get_option('buildtype') == 'release' or get_option('buildtype') == 'debugoptimized'
  add_project_arguments('-DNDEBUG', language: 'cpp')
endif


executable('slip', sources, 
		   dependencies : [readline, boost, std],
		   install: true,
		   install_dir: meson.source_root() )
