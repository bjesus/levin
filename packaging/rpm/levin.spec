Name:           levin
Version:        0.1.0
Release:        1%{?dist}
Summary:        BitTorrent archive daemon for distributed content preservation

License:        MIT
URL:            https://github.com/bjesus/levin
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.15
BuildRequires:  gcc-c++ >= 9
BuildRequires:  boost-devel >= 1.70
BuildRequires:  openssl-devel
BuildRequires:  libcurl-devel
BuildRequires:  rb_libtorrent-devel >= 2.0
BuildRequires:  pkgconfig
BuildRequires:  rpm-build
BuildRequires:  rpmdevtools
BuildRequires:  systemd-rpm-macros

%description
Levin is a lightweight BitTorrent daemon designed for archival and content
preservation. It monitors directories for torrent files and automatically
downloads and seeds content while managing disk space efficiently.

Key features:
* Automatic torrent monitoring and management
* Intelligent disk space management
* Command-line interface for control
* Statistics and monitoring
* User-level installation (no root required)

%prep
%autosetup

%build
%cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
%cmake_build

%install
%cmake_install

# Install systemd user service
install -D -m 644 scripts/levin.service %{buildroot}%{_userunitdir}/levin.service

%post
echo ""
echo "Levin has been installed successfully!"
echo ""
echo "To get started:"
echo "  1. Run 'levin' once to create default configuration"
echo "  2. Edit ~/.config/levin/levin.toml to customize settings"
echo "  3. Enable the service: systemctl --user enable levin"
echo "  4. Start the service: systemctl --user start levin"
echo ""
echo "For more information, visit: https://github.com/bjesus/levin"
echo ""

%preun
if [ $1 -eq 0 ]; then
    # Package removal (not upgrade)
    systemctl --user stop levin.service >/dev/null 2>&1 || :
    systemctl --user disable levin.service >/dev/null 2>&1 || :
fi

%files
%license LICENSE
%doc README.md
%{_bindir}/levin
%{_userunitdir}/levin.service

%changelog
* Wed Dec 25 2024 bjesus <bjesus@users.noreply.github.com> - 0.2.2-1
- Fix RPM packaging and release workflow
- Add missing systemd-rpm-macros dependency
- Auto-update version from git tags

* Wed Dec 25 2024 bjesus <bjesus@users.noreply.github.com> - 0.2.1-1
- Android bug fixes
- Fix lazy libtorrent session initialization
- Fix default settings for better battery life
- Fix version number display

* Tue Dec 24 2024 bjesus <bjesus@users.noreply.github.com> - 0.2.0-1
- Unified storage management across platforms
- Add min_free and max_storage constraints
- Support human-readable storage format
- Enhanced Android statistics display
- Boot receiver for auto-start on Android

* Mon Dec 23 2024 bjesus <bjesus@users.noreply.github.com> - 0.1.0-1
- Initial release
- BitTorrent archive daemon with intelligent disk management
- User-level installation support
- XDG-compliant directory structure
- Command-line interface for monitoring and control
- Automatic torrent monitoring
- Statistics tracking
