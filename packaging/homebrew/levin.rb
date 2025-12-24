class Levin < Formula
  desc "BitTorrent archive daemon for distributed content preservation"
  homepage "https://github.com/bjesus/levin"
  url "https://github.com/bjesus/levin/archive/v0.1.0.tar.gz"
  sha256 "" # Will be filled after first release
  license "MIT"
  head "https://github.com/bjesus/levin.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "pkg-config" => :build
  depends_on "boost"
  depends_on "libtorrent-rasterbar"
  depends_on "openssl@3"

  def install
    system "cmake", "-S", ".", "-B", "build",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DBUILD_TESTS=OFF",
                    *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"

    # Install brew-compatible service file (different from Linux)
    (prefix/"homebrew.levin.plist").write plist_contents
  end

  def plist_contents
    <<~EOS
      <?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
      <plist version="1.0">
      <dict>
        <key>Label</key>
        <string>#{plist_name}</string>
        <key>ProgramArguments</key>
        <array>
          <string>#{opt_bin}/levind</string>
          <string>--config</string>
          <string>#{Dir.home}/.config/levin/levin.toml</string>
          <string>--foreground</string>
        </array>
        <key>RunAtLoad</key>
        <false/>
        <key>KeepAlive</key>
        <true/>
        <key>StandardOutPath</key>
        <string>#{Dir.home}/.local/state/levin/stdout.log</string>
        <key>StandardErrorPath</key>
        <string>#{Dir.home}/.local/state/levin/stderr.log</string>
      </dict>
      </plist>
    EOS
  end

  def caveats
    <<~EOS
      Levin has been installed successfully!

      To get started:
        1. Run 'levind' once to create default configuration
        2. Edit ~/.config/levin/levin.toml to customize settings
        3. Start the service: brew services start levin
        
      To run manually:
        levind --config ~/.config/levin/levin.toml --foreground

      For more information, visit: https://github.com/bjesus/levin
    EOS
  end

  test do
    # Test version flag
    assert_match "Levin v#{version}", shell_output("#{bin}/levind --version")
    assert_match "Levin v#{version}", shell_output("#{bin}/levin --version")
  end
end
