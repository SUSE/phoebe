test_script = find_program(meson.current_source_dir() / 'run_tests.sh')

test(
  'integration test',
  test_script,
  args : [meson.project_source_root(), meson.project_build_root()]
)
