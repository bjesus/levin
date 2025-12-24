# Homebrew Tap Setup for Levin

This directory contains the Homebrew formula for Levin. To set up the official tap, you'll need to create a separate repository.

## Creating the Homebrew Tap Repository

1. **Create a new GitHub repository** named `homebrew-levin`:
   ```bash
   # On GitHub, create: github.com/bjesus/homebrew-levin
   ```

2. **Clone and set up the tap repository**:
   ```bash
   git clone https://github.com/bjesus/homebrew-levin.git
   cd homebrew-levin
   mkdir -p Formula
   cp /path/to/levin/homebrew/levin.rb Formula/
   git add Formula/levin.rb
   git commit -m "Add Levin formula"
   git push
   ```

3. **Update the SHA256 checksum** after creating v0.1.0 release:
   ```bash
   # Download the release tarball
   wget https://github.com/bjesus/levin/archive/v0.1.0.tar.gz
   
   # Calculate SHA256
   shasum -a 256 v0.1.0.tar.gz
   
   # Update the sha256 field in Formula/levin.rb
   ```

## Installation for Users

Once the tap is set up, users can install Levin with:

```bash
# Add the tap
brew tap bjesus/levin

# Install levin
brew install levin

# Start the service
levind  # First run to create config
brew services start levin
```

## Testing the Formula Locally

Before publishing, test the formula:

```bash
# Audit the formula
brew audit --strict Formula/levin.rb

# Test installation locally
brew install --build-from-source Formula/levin.rb

# Run tests
brew test levin
```

## macOS-Only

Note: This Homebrew tap is for macOS only. Linux users should use native packages (.deb, .rpm, AUR) instead.
