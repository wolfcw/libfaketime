require 'formula'

class Libfaketime < Formula
  homepage 'https://github.com/wolfcw/libfaketime'
  url 'https://github.com/wolfcw/libfaketime/archive/v0.9.7b1.tar.gz'
  sha1 'f38fe2b355cdfc74807646707a2f2c3e3be84032'

  depends_on :macos => :sierra

  fails_with :llvm do
    build 2336
    cause 'No thread local storage support'
  end

  def install
    system "make", "-C", "src", "-f", "Makefile.OSX", "PREFIX=#{prefix}"
    bin.install 'src/faketime'
    (lib/'faketime').install 'src/libfaketime.1.dylib'
    man1.install 'man/faketime.1'
  end
end

