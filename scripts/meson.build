prog_python = import('python').find_installation(
  'python3',
  required : get_option('collector'),
  modules : ['cffi'],
  disabler : true,
)
collector_script = files('collect_stats.py')

abi_src = custom_target(
  '_phoebe.c',
  input : [collector_script],
  output : ['_phoebe.c'],
  command : [prog_python, '@INPUT@', 'generate', '@OUTPUT@'],
)
abi = prog_python.extension_module(
  '_phoebe',
  abi_src, stat_src, common_src,
  dependencies : [prog_python.dependency(embed : true), common_dep, nl_nf_3],
  install: true,
  install_dir : prog_python.get_install_dir()
)
