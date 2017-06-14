

project('spam', 'cpp', default_options : ['std=c++11'])


sources = ['spam.cpp']
python2 = dependency('python2')

shared_module('spam', sources, name_prefix : '', name_suffix : 'so',
              dependencies: python2)

