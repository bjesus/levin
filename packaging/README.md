# Levin Packaging

This directory contains all packaging-related files for distributing Levin across different platforms.

## Directory Structure

```
packaging/
├── debian/          # Debian/Ubuntu packages (.deb)
├── rpm/             # Fedora/RHEL packages (.rpm)
├── aur/             # Arch Linux AUR
├── homebrew/        # macOS Homebrew formula
└── README.md        # This file
```

## Platform-Specific Instructions

### Debian/Ubuntu (.deb)

**Location:** `debian/`

**Build locally:**
```bash
# From project root
ln -s packaging/debian debian
dpkg-buildpackage -us -uc -b
```

**Files:**
- `control` - Package metadata and dependencies
- `rules` - Build instructions
- `install` - File installation list
- `postinst` - Post-installation script
- `prerm` - Pre-removal script
- `copyright` - License information
- `changelog` - Version history
- `compat` - Debhelper compatibility level
- `source/format` - Source package format

**Supported versions:**
- Ubuntu 20.04 LTS (Focal)
- Ubuntu 22.04 LTS (Jammy)
- Ubuntu 24.04 LTS (Noble)
- Debian 11 (Bullseye)
- Debian 12 (Bookworm)

### Fedora/RHEL (.rpm)

**Location:** `rpm/`

**Build locally:**
```bash
# Set up RPM build tree
rpmdev-setuptree

# Copy spec file
cp packaging/rpm/levin.spec ~/rpmbuild/SPECS/

# Create source tarball
tar czf ~/rpmbuild/SOURCES/levin-0.1.0.tar.gz \
  --transform 's,^,levin-0.1.0/,' \
  --exclude='.git' \
  --exclude='build' \
  .

# Build
cd ~/rpmbuild/SPECS
rpmbuild -bb levin.spec
```

**Files:**
- `levin.spec` - RPM specification file

**Supported versions:**
- Fedora 39, 40
- RHEL 9
- CentOS Stream 9

### Arch Linux (AUR)

**Location:** `aur/`

**Submit to AUR:**
```bash
# Clone AUR repository (first time)
git clone ssh://aur@aur.archlinux.org/levin.git aur-levin
cd aur-levin

# Copy files
cp ../packaging/aur/PKGBUILD .
cp ../packaging/aur/levin.install .

# Update .SRCINFO
makepkg --printsrcinfo > .SRCINFO

# Commit and push
git add PKGBUILD levin.install .SRCINFO
git commit -m "Update to version X.Y.Z"
git push
```

**Build locally:**
```bash
cd packaging/aur
makepkg -si
```

**Files:**
- `PKGBUILD` - Build script
- `levin.install` - Install/upgrade/removal hooks

### macOS (Homebrew)

**Location:** `homebrew/`

**Create tap repository:**

The Homebrew formula should be published in a separate tap repository:
`github.com/bjesus/homebrew-levin`

```bash
# Create and clone tap repository
gh repo create homebrew-levin --public
git clone https://github.com/bjesus/homebrew-levin.git

# Copy formula
mkdir -p homebrew-levin/Formula
cp packaging/homebrew/levin.rb homebrew-levin/Formula/

# Commit and push
cd homebrew-levin
git add Formula/levin.rb
git commit -m "Add Levin formula"
git push
```

**Update SHA256 after release:**
```bash
# Download release tarball
wget https://github.com/bjesus/levin/archive/v0.1.0.tar.gz

# Calculate SHA256
shasum -a 256 v0.1.0.tar.gz

# Update sha256 field in Formula/levin.rb
```

**Test locally:**
```bash
brew install --build-from-source packaging/homebrew/levin.rb
brew test levin
brew audit --strict packaging/homebrew/levin.rb
```

**Files:**
- `levin.rb` - Homebrew formula
- `README.md` - Tap setup instructions

## CI/CD Integration

All packaging is automated via GitHub Actions (`.github/workflows/release.yml`).

On tag push (`v*`), the workflow:
1. Builds .deb for Ubuntu 20.04, 22.04, 24.04
2. Builds .rpm for Fedora 39, 40
3. Builds macOS binary tarball
4. Creates GitHub Release with all artifacts
5. Generates release notes with installation instructions

**Manual steps after automated release:**
1. Update AUR package with new version and SHA256
2. Update Homebrew tap SHA256 (or automate with workflow)

## Building All Packages Locally

### Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt-get install devscripts debhelper
```

**Fedora:**
```bash
sudo dnf install rpm-build rpmdevtools
```

**Arch:**
```bash
sudo pacman -S base-devel
```

### Build Script

Create a build script to build all packages:

```bash
#!/bin/bash
# build-all-packages.sh

set -e

VERSION="0.1.0"
PROJECT_ROOT=$(pwd)

# Build Debian package
echo "Building Debian package..."
dpkg-buildpackage -us -uc -b
mv ../levin_*.deb dist/

# Build RPM package
echo "Building RPM package..."
rpmdev-setuptree
cp packaging/rpm/levin.spec ~/rpmbuild/SPECS/
tar czf ~/rpmbuild/SOURCES/levin-${VERSION}.tar.gz \
  --transform "s,^,levin-${VERSION}/," \
  --exclude='.git' --exclude='build' .
rpmbuild -bb ~/rpmbuild/SPECS/levin.spec
cp ~/rpmbuild/RPMS/*/*.rpm dist/

# Build AUR package (test only)
echo "Building AUR package..."
cd packaging/aur
makepkg --clean
cp levin-*.pkg.tar.zst ../../dist/
cd ${PROJECT_ROOT}

echo "All packages built successfully!"
ls -lh dist/
```

## Version Updates

When releasing a new version, update:

1. **CMakeLists.txt** - `project(levin VERSION X.Y.Z)`
2. **packaging/debian/changelog** - Add new entry
3. **packaging/rpm/levin.spec** - Update `Version:` and `%changelog`
4. **packaging/aur/PKGBUILD** - Update `pkgver=` and `pkgrel=`
5. **packaging/homebrew/levin.rb** - Update `version` and `sha256`

## Testing Packages

### Debian/Ubuntu

```bash
# Install in container
docker run -it ubuntu:22.04 bash
apt-get update && apt-get install -y ./levin_*.deb
levin --version
systemctl --user status levin
```

### Fedora/RHEL

```bash
# Install in container
docker run -it fedora:40 bash
dnf install -y ./levin-*.rpm
levin --version
systemctl --user status levin
```

### Arch Linux

```bash
# Install locally
sudo pacman -U levin-*.pkg.tar.zst
levin --version
systemctl --user status levin
```

### macOS

```bash
# Test formula
brew install --build-from-source packaging/homebrew/levin.rb
levin --version
brew services list
```

## Troubleshooting

### Debian build fails

```bash
# Check dependencies
dpkg-checkbuilddeps

# Install missing deps
sudo apt-get build-dep .
```

### RPM build fails

```bash
# Check spec file
rpmlint packaging/rpm/levin.spec

# Check for missing dependencies
rpmdev-checkrequires ~/rpmbuild/RPMS/*/levin-*.rpm
```

### AUR validation fails

```bash
# Validate PKGBUILD
namcap packaging/aur/PKGBUILD

# Check package
makepkg -f
namcap levin-*.pkg.tar.zst
```

### Homebrew audit fails

```bash
# Run audit
brew audit --strict packaging/homebrew/levin.rb

# Fix common issues:
# - Update sha256 after release
# - Check formula syntax
# - Verify URLs are accessible
```

## Contributing

When adding support for new platforms or updating packaging:

1. Test builds locally
2. Update this README
3. Update CI/CD workflows if needed
4. Document any platform-specific requirements

## Resources

- [Debian Policy Manual](https://www.debian.org/doc/debian-policy/)
- [RPM Packaging Guide](https://rpm-packaging-guide.github.io/)
- [Arch Package Guidelines](https://wiki.archlinux.org/title/Arch_package_guidelines)
- [Homebrew Formula Cookbook](https://docs.brew.sh/Formula-Cookbook)
