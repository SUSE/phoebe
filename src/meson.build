common_src = [
  meson.current_source_dir() / 'filehelper.c',
  meson.current_source_dir() / 'utils.c'
]
common_dep = declare_dependency(
  dependencies : [nl3, json_c, pthread, m, dl],
  include_directories : incdir
)

plugins = [
  shared_library(
    'network_plugin',
    ['network_plugin.c', 'stats.c', 'algorithmic.c'] + common_src,
    dependencies : [nl_nf_3, common_dep]
  )
]

phoebe = executable(
  'phoebe',
  ['phoebe.c'] + common_src,
  dependencies : common_dep
)
