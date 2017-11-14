require 'formula'

class Libfaketime < Formula
  homepage 'https://github.com/wolfcw/libfaketime'
  url 'https://github.com/wolfcw/libfaketime/archive/v0.9.7.tar.gz'
  sha1 'eb1cbacf548aefa36214bea1345f35b8763027a1'

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
