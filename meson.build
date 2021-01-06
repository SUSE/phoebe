project('Phoeβe', 'c', default_options : ['c_std=gnu99', 'warning_level=2'])

incdir = include_directories('headers')

nl3 = dependency('libnl-3.0')
nl_nf_3 = dependency('libnl-nf-3.0')
json_c = dependency('json-c')
pthread = dependency('threads')

cc = meson.get_compiler('c')
m = cc.find_library('m', required : true)
dl = cc.find_library('dl', required : true)

add_project_arguments('-DPRINT_MSGS', language : 'c')

subdir('src')
subdir('scripts')
subdir('tests')
