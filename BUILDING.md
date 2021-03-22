# Building
The code is built using [Meson](https://mesonbuild.com/):

```ShellSession
$ meson build
$ cd build/
$ meson compile
```

You can also run debug builds using address or undefined behavior sanitizer:

```ShellSession
$ meson build -Db_sanitize=undefined # or -Db_sanitize=address for ASAN
$ cd build/
$ meson compile
```

There are few compile-time flags which can be passed to Meson to enable some code behavior:

* **print_messages**: used to print to `stdout` only the most important messages (this is the only parameter enabled by default)
* **print_advanced_messages**: used for very verbose printing to `stdout` (useful for debugging purposes)
* **print_table**: used to print to `stdout` all data stored in the different tables maintained by the application
* **apply_changes**: this enables the application to actually apply the settings via `sysctl`/`ethtool` command
* **check_initial_settings**: when enabled, this will prevent the application from applying lower settings than the ones already applied to the system at bootstrap
* **m_threads**: when enabled, this will run training using as many threads as available cores on the machine

These flags can be enabled by passing them to the Meson configure step:

```ShellSession
$ meson -Dprint_advanced_messages=true -Dprint_table=true build
```

