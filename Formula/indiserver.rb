class Indiserver < Formula
  desc "Instrument-Neutral Distributed Interface server"
  homepage "https://www.indilib.org/"
  url "https://github.com/indilib/indi/archive/refs/tags/v2.2.4.2.tar.gz"
  sha256 "025bf3d5e4edebae86851d3a5d66ba4596a2c3a49f861f7c49d99b421dadf102"
  license "GPL-2.0-or-later"

  depends_on "cmake" => :build
  depends_on "libev"
  depends_on "libnova"

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args,
           "-DINDI_BUILD_SERVER=ON",
           "-DINDI_BUILD_CLIENT=OFF",
           "-DINDI_BUILD_QT_CLIENT=OFF",
           "-DINDI_BUILD_DRIVERS=OFF",
           "-DINDI_BUILD_COMMON=OFF",
           "-DINDI_BUILD_EXAMPLES=OFF",
           "-DINDI_BUILD_UNITTESTS=OFF",
           "-DINDI_BUILD_INTEGTESTS=OFF"
    system "cmake", "--build", "build"
    system "cmake", "--install", "build", "--component", "Unspecified"
  end

  test do
    output = shell_output("#{bin}/indiserver -h 2>&1", 2)
    assert_match "Usage:", output
  end
end
