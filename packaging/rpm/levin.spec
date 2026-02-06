Name:           levin
Version:        0.1.0
Release:        1%{?dist}
Summary:        Background torrent seeding client for Anna's Archive

License:        MIT
URL:            https://annas-archive.org
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++ >= 9
BuildRequires:  libcurl-devel
BuildRequires:  openssl-devel

Requires:       libcurl
Requires:       openssl-libs

%description
Levin is a lightweight daemon that downloads and seeds torrents from
Anna's Archive. It respects device constraints by pausing on battery
power, managing disk space automatically, and supporting WebTorrent
for browser-based downloads.

%prep
%autosetup

%build
%cmake -DLEVIN_BUILD_TESTS=OFF -DLEVIN_USE_STUB_SESSION=OFF
%cmake_build

%install
install -Dm755 %{_vpath_builddir}/platforms/linux/levin \
    %{buildroot}%{_bindir}/levin

install -Dm644 packaging/systemd/levin.service \
    %{buildroot}%{_userunitdir}/levin.service

# Default config directory
install -dm755 %{buildroot}%{_sysconfdir}/skel/.config/levin/torrents

%post
echo "To enable Levin as a user service:"
echo "  systemctl --user daemon-reload"
echo "  systemctl --user enable --now levin"

%preun
# Stop user service if running (best effort)
systemctl --user stop levin 2>/dev/null || true
systemctl --user disable levin 2>/dev/null || true

%files
%{_bindir}/levin
%{_userunitdir}/levin.service
%dir %{_sysconfdir}/skel/.config/levin
%dir %{_sysconfdir}/skel/.config/levin/torrents

%changelog
* Thu Feb 06 2026 Levin Contributors <levin@annas-archive.org> - 0.1.0-1
- Initial package
