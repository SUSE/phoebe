common_src = files('filehelper.c', 'utils.c')
stat_src = files('stats.c')

common_dep = declare_dependency(
  dependencies : [nl3, json_c, pthread, m, dl],
  include_directories : incdir
)

plugins = [
  shared_library(
    'network_plugin',
    ['network_plugin.c', 'stats.c', 'algorithmic.c'], common_src,
    dependencies : [nl_nf_3, common_dep],
    install : true,
    install_dir : get_option('libdir') / meson.project_name()
  )
]

phoebe = executable(
  'phoebe',
  'phoebe.c', common_src,
  dependencies : common_dep,
  install : true,
  install_dir : get_option('bindir')
)

data_tool = executable(
  'data_tool',
  'data_tool.c',
  install : true,
  install_dir : get_option('bindir')
)
