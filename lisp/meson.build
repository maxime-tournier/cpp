
project('lisp', 'cpp', default_options : ['cpp_std=c++11'])

sources = ['main.cpp', 'eval.cpp', 'value.cpp', 'parse.cpp' ]

compiler = meson.get_compiler('cpp')
readline = compiler.find_library('readline', required: true)

if get_option('buildtype') == 'release'
add_project_arguments('-DNDEBUG', language: 'cpp')
endif


executable('lisp', sources, 
		   dependencies : [readline],
		   install: true,
		   install_dir: meson.source_root() )
