# Phoebe build dependencies

## openSUSE Tumbleweed, Leap15.2
```bash
zypper -n in gcc clang make libnetlink-devel libnl3-devel libjson-c-devel meson ethtool python3 python3-devel python3-cffi git valgrind libcmocka-devel
```

## SUSE SLES:15:SP2
```bash
zypper -n in gcc clang make libnetlink-devel libnl3-devel libjson-c-devel meson ethtool python3 python3-devel python3-cffi git valgrind libcmocka-devel
```



## Debian 10 (Buster)

```bash
sudo apt install gcc clang make libnl-3-dev libnl-nf-3-dev libjson-c-dev ethtool pkg-config python3 python3-pip python3-cffi python3-dev git valgrind libcmocka-dev

pip3 install meson ninja
```

The version of meson in Debian's repositories is out of date and not compatible. Installing via pip installs a version that works well

