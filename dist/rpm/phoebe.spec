#
# spec file for package phoebe
#
# Copyright (c) 2021 SUSE LLC
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via https://bugs.opensuse.org/
#

Name:           phoebe
Version:        0.0.4
Release:        0.3
Summary:        Phoeβe wants to add basic artificial intelligence capabilities to the Linux OS
License:        BSD-3-Clause
URL:            https://github.com/SUSE/phoebe
Source:         phoebe-%version.tar.xz
BuildRequires:  meson
BuildRequires:  ninja
BuildRequires:  python3-devel
BuildRequires:  python3-cffi
BuildRequires:  python3-pycparser
BuildRequires:  libjson-c-devel
BuildRequires:  libnl3-devel
Group:          System/Benchmark

%description

System-level tuning is a very complex activity, requiring the knowledge and expertise of several (all?) layers which compose the system itself, how they interact with each other and (quite often) it is required to also have an intimate knowledge of
the implementation of the various layers.

Another big aspect of running systems is dealing with failure. Do not think of failure as a machine turning on fire rather as an overloaded system, caused by misconfiguration, which could lead to starvation of the available resources.

In many circumstances, operators are used to deal with telemetry, live charts, alerts, etc. which could help them identifying the offending machine(s) and (re)act to fix any potential issues.

However, one question comes to mind: wouldn't it be awesome if the machine could auto-tune itself and provide a self-healing capability to the user? Well, if that is enough to trigger your interest then this is what Phoeβe aims to provide.

Phoeβe uses system telemetry as the input to its brain and produces a big set of settings which get applied to the running system. The decision made by the brain is continuously reevaluated (considering the grace_period setting)
to offer eventually the best possible setup.

%prep
%setup -q
meson build

%build
cd build
meson compile

%install
mkdir -p /${RPM_BUILD_ROOT}/%{_bindir}
mkdir -p /${RPM_BUILD_ROOT}/%{_libdir}/phoebe
mkdir -p /${RPM_BUILD_ROOT}/%{_sysconfdir}/phoebe
mkdir -p /${RPM_BUILD_ROOT}/%{_datadir}/phoebe
install -m 755 build/src/phoebe                 /${RPM_BUILD_ROOT}/%{_bindir}
install -m 644 build/src/libnetwork_plugin.so   /${RPM_BUILD_ROOT}/%{_libdir}/phoebe
install -m 644 settings.json                    /${RPM_BUILD_ROOT}/%{_sysconfdir}/phoebe/settings.json
install -m 644 csv_files/rates.csv              /${RPM_BUILD_ROOT}/%{_datadir}/phoebe/rates.csv

%files
%license LICENSE
%doc OWNERS README.md CONTRIBUTING.md SETUP.md
%{_bindir}/phoebe
%{_libdir}/phoebe/
%{_datadir}/phoebe/
%{_sysconfdir}/phoebe/

%changelog
