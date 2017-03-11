project('pouf', 'cpp', default_options : ['cpp_std=c++11'])

eigen = dependency('eigen3')
python = dependency('python')
boost = dependency('boost', modules : ['python'])

deps = [eigen, python, boost]

sources = ['python.cpp',	
		'python/module.cpp',
		'python/node.cpp',
		'python/dofs.cpp',
		'python/metric.cpp',
		'python/graph.cpp']		

library('pouf', sources, name_prefix : '', dependencies : deps, install: true, install_dir: meson.source_root() )
