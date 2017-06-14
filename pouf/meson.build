
project('pouf', 'cpp', default_options : ['std=c++11'])

eigen = dependency('eigen3')

boost = dependency('boost', modules : ['graph', 'python'], version : '>=1.63')

python = dependency('python')

python_sources = ['python.cpp',	
		'python/module.cpp',
		'python/numpy.cpp',
		'python/dofs.cpp',
		'python/metric.cpp',
		'python/func.cpp',
		'python/graph.cpp',
]		

sources = [	'uniform.cpp' ]

r = run_command('python', '-c', 'import numpy; print numpy.get_include()')

numpy_include = r.stdout().strip()
message('numpy include: ' + numpy_include)
numpy = declare_dependency( include_directories : include_directories(numpy_include) )

deps = [eigen, python, boost, numpy]

shared_module('pouf', sources + python_sources, name_prefix : '', name_suffix : 'so',
			  dependencies : deps,
			  install: true,
			  install_dir: meson.source_root() )


executable('pouf', ['main.cpp'] + sources, 
		   dependencies : [eigen],
		   install: true,
		   install_dir: meson.source_root() )
