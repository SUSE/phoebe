prog_python = import('python').find_installation('python3')

abi_src = custom_target(
  '_phoebe.c',
  input : ['collect_stats.py'],
  output : ['_phoebe.c'],
  command : [prog_python, '@INPUT@', 'generate', '@OUTPUT@']
)
abi = shared_library(
  '_phoebe.abi3',
  abi_src, stat_src, common_src,
  name_prefix : '',
  dependencies : [prog_python.dependency(embed : true), common_dep, nl_nf_3]
)
