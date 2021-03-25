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
Version:        0.1
Release:        0
Summary:        Phoeβe wants to add basic artificial intelligence capabilities to the Linux OS
License:        BSD-3-Clause
URL:            https://github.com/SUSE/%{name}
Source:         %{URL}/archive/v%{version}.tar.gz#/%{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(json-c)
BuildRequires:  pkgconfig(libnl-3.0)
BuildRequires:  pkgconfig(libnl-nf-3.0)
BuildRequires:  meson
BuildRequires:  gcc
BuildRequires:  python3-cffi
BuildRequires:  python3-devel
BuildRequires:  python3-pycparser

%description

System-level tuning is a very complex activity, requiring the knowledge and expertise of several (all?) layers which compose the system itself, how they interact with each other and (quite often) it is required to also have an intimate knowledge of
the implementation of the various layers.

Another big aspect of running systems is dealing with failure. Do not think of failure as a machine turning on fire rather as an overloaded system, caused by misconfiguration, which could lead to starvation of the available resources.

In many circumstances, operators are used to deal with telemetry, live charts, alerts, etc. which could help them identifying the offending machine(s) and (re)act to fix any potential issues.

However, one question comes to mind: wouldn't it be awesome if the machine could auto-tune itself and provide a self-healing capability to the user? Well, if that is enough to trigger your interest then this is what Phoeβe aims to provide.

Phoeβe uses system telemetry as the input to its brain and produces a big set of settings which get applied to the running system. The decision made by the brain is continuously reevaluated (considering the grace_period setting)
to offer eventually the best possible setup.

%prep
%autosetup -p1

%build
# export build flags manually if %%set_build_flags is not defined
%{?!set_build_flags:export CFLAGS="%{optflags}"; export LDFLAGS="${RPM_LD_FLAGS}"}
%meson -Dwith_tests=false
%meson_build

%install
%meson_install

%files
%license LICENSE
%{_bindir}/%{name}
%{_bindir}/data_tool
%dir %{_libdir}/%{name}
%dir %{_datadir}/%{name}
%dir %{_sysconfdir}/%{name}
%{_libdir}/%{name}/libnetwork_plugin.so
%{_datadir}/%{name}/rates.csv
%config(noreplace) %{_sysconfdir}/%{name}/settings.json

%changelog
