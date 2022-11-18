{ lib
, cmake
, gnumake
, gcc12_multi
, bintools_multi
, nasm
, qemu
, mtools
, gnutar }:

gcc12_multi.stdenv.mkDerivation rec {
  pname = "hhuOS";
  version = "experimental";
  
  src = ./.;

  nativeBuildInputs = [ cmake, nasm, mtools, gnutar ];
  buildInputs = [ qemu ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Default" # Default/Debug
  ];

  meta = with lib; {
    homepage = "https://github.com/churl/hhuos";
    description = ''
      hhuOS - A small operating system
    '';
    licencse = licenses.gpl3Only;
    platforms = platforms.linux;
  };
}
