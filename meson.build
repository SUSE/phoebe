project('phoebe', 'c', default_options : ['c_std=gnu99', 'warning_level=2'])

incdir = include_directories('headers')

nl3 = dependency('libnl-3.0')
nl_nf_3 = dependency('libnl-nf-3.0')
json_c = dependency('json-c')
pthread = dependency('threads')

cc = meson.get_compiler('c')
m = cc.find_library('m', required : true)
dl = cc.find_library('dl', required : true)

options = [
  ['print_messages', 'PRINT_MSGS'],
  ['print_advanced_messages', 'PRINT_ADV_MSGS'],
  ['print_table', 'PRINT_TABLE'],
  ['apply_changes', 'APPLY_CHANGES'],
  ['check_initial_settings', 'CHECK_INITIAL_SETTINGS'],
  ['m_threads', 'M_THREADS'],
  ['use_linear_regression', 'LINEAR_REGRESSION']
]

if get_option('b_sanitize') == 'undefined'
  add_global_arguments('-fno-sanitize-recover', language : 'c')
endif

if cc.get_id() == 'clang' and (
  get_option('b_sanitize') == 'undefined' or get_option('b_sanitize') == 'address'
)
  add_project_link_arguments('-shared-libasan', language : 'c')
endif

foreach opt : options
  if get_option(opt[0])
    add_project_arguments('-D@0@'.format(opt[1]), language : 'c')
  endif
endforeach

systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_pkgconfig_variable('systemdsystemunitdir')
unit_files = [
    'phoebe-network.service'
]

subdir('conf')
subdir('src')
subdir('scripts')
if get_option('with_tests')
  subdir('tests')
endif
subdir('csv_files')

foreach u : unit_files
    configure_file(
        copy: true,
        input: u,
        install: true,
        install_dir: systemd_system_unit_dir,
        output: u,
    )
endforeach
