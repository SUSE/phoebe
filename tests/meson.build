setup_script = find_program(meson.current_source_dir() / 'setup.sh')

# When building with clang we use -shared-libasan, but clang's shared ASAS/UBSAN
# is not in the default library search path. Thus we must add it manually to
# LD_LIBRARY_PATH and check first that the library is actually where we expect
# it to be
if cc.get_id() == 'clang'
  base_path = '/usr' / get_option('libdir') / 'clang' / cc.version() / 'lib/linux/'
  libclang_rt = cc.find_library(base_path / 'libclang_rt.asan-@0@'.format(build_machine.cpu_family()))
  env = ['LD_LIBRARY_PATH=@0@'.format(base_path)]
else
  env = []
endif

if meson.version().version_compare('>=0.56.0')
  args = [meson.project_source_root(), meson.project_build_root()]
else
  args = [meson.source_root(), meson.build_root()]
endif

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
#  env : env,
#)
