setup_script = find_program(meson.current_source_dir() / 'setup.sh')
find_libasan_script = find_program(meson.current_source_dir() / 'find_gcc_libasan.sh')

# When building with clang we use -shared-libasan, but clang's shared ASAS/UBSAN
# is not in the default library search path. Thus we must add it manually to
# LD_LIBRARY_PATH and check first that the library is actually where we expect
# it to be
if cc.get_id() == 'clang'
  base_path = '/usr' / get_option('libdir') / 'clang' / cc.version() / 'lib/linux/'
  env = {'LD_LIBRARY_PATH': base_path}
else
  env = {}
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
  libasan = run_command(find_libasan_script).stdout().strip()
else
  error('Unsupported compiler to use with address sanitizer')
endif

if meson.version().version_compare('>=0.56.0')
  args = [meson.project_source_root(), meson.project_build_root()]
else
  args = [meson.source_root(), meson.build_root()]
endif

python_env = env + {
  'PYTHONPATH': args[1] / 'scripts',
  'LD_PRELOAD': libasan,
  'ASAN_OPTIONS': 'detect_leaks=0',
}

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
test(
  'integration test (collector)',
  collector_script,
  args : ['lo', '1'],
  depends : [abi],
  env : python_env,
)

cmocka = dependency('cmocka')

unit_tests = executable(
  'unit_tests',
  ['unit_tests.c', 'test_filehelper.c'] + common_src,
  dependencies : [cmocka, common_dep],
  link_args : ['-Wl,--wrap=feof', '-Wl,--wrap=fgetc']
)
test('unit tests', unit_tests, env : env)
