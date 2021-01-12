setup_script = find_program(meson.current_source_dir() / 'setup.sh')

# When building with clang we use -shared-libasan, but clang's shared ASAS/UBSAN
# is not in the default library search path. Thus we must add it manually to
# LD_LIBRARY_PATH and check first that the library is actually where we expect
# it to be
if cc.get_id() == 'clang'
  base_path = '/usr' / get_option('libdir') / 'clang' / cc.version() / 'lib/linux/'
  env = ['LD_LIBRARY_PATH=@0@'.format(base_path)]
else
  env = []
endif

# ASAN requies its runtime to be loaded first, which is a problem when we run
# collect_stats.py since the Python executable that comes with the distro won't
# do that. So we find the ASAN runtime manually here and add it to LD_PRELOAD
# later.
if not get_option('b_sanitize').contains('address')
  libasan = ''
elif cc.get_id() == 'clang'
  libasan = run_command(
    cc,
    '-print-file-name=libclang_rt.asan-@0@.so'.format(build_machine.cpu_family()),
  ).stdout().strip()
elif cc.get_id() == 'gcc'
  libasan = run_command(cc, '-print-file-name=libasan.so').stdout().strip()
else
  error('Unsupported compiler to use with address sanitizer')
endif

if meson.version().version_compare('>=0.56.0')
  args = [meson.project_source_root(), meson.project_build_root()]
else
  args = [meson.source_root(), meson.build_root()]
endif

python_env = env + [
  'PYTHONPATH=@0@'.format(args[1] / 'scripts'),
  'LD_PRELOAD=@0@'.format(libasan),
  'ASAN_OPTIONS=detect_leaks=0',
]

test(
  'integration test setup',
  setup_script,
  args : args,
  priority : 1000,
  is_parallel : false
)
test(
  'integration test (main)',
  phoebe,
  args : ['-s', 'settings.json', '-f', 'rates.csv', '-m', 'training'],
  env : env,
)
# disabled until we solve https://github.com/SUSE/phoebe/issues/2
#test(
#  'integration test (collector)',
#  collector_script,
#  args : ['lo', '1'],
#  depends : [abi],
#  env : python_env,
#)
