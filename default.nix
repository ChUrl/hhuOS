{ lib
, fetchgit
, cmake
, gnumake
, gcc12_multi
, bintools_multi
, nasm
, qemu
, mtools
, gnutar }:

let
  lvgl = gcc12_multi.stdenv.mkDerivation rec {
    name = "lvgl-${version}";
    version = "8.3.3";

    src = fetchgit {
      url = "https://github.com/lvgl/lvgl.git";
      rev = "refs/tags/v8.3.3";
      sha256 = "sha256-GdddMRq7QKYzrpSilo+sFslgspt3iM1RPL28EFjNREE=";
    };

    nativeBuildInputs = [ cmake bintools_multi ];

    cmakeFlags = [
      "-DCMAKE_C_COMPILER=${gcc12_multi}/bin/gcc"
      "-DCMAKE_CXX_COMPILER=${gcc12_multi}/bin/gcc"
    ];

    # This is ugly af, but lvgl doesn't provide a lvgl-config.cmake file
    # A better way (probably) would be to provide a patch for lvgl's env_support/cmake/custom.cmake and a lvgl-config.cmake.in to generate this file with cmake...
    # https://stackoverflow.com/questions/65045150/how-to-fix-could-not-find-a-package-configuration-file-error-in-cmake
    postInstall = ''
      mkdir -p $out/lib/cmake/lvgl

      echo "find_path(lvgl_INCLUDE_DIR lvgl/lvgl.h)"                                                                                                                                                >> $out/lib/cmake/lvgl/lvgl-config.cmake
      echo "find_library(lvgl_LIBRARY_DIR lvgl)"                                                                                                                                                    >> $out/lib/cmake/lvgl/lvgl-config.cmake

      echo "include(FindPackageHandleStandardArgs)"                                                                                                                                                 >> $out/lib/cmake/lvgl/lvgl-config.cmake
      echo "find_package_handle_standard_args(lvgl REQUIRED_VARS lvgl_INCLUDE_DIR lvgl_LIBRARY_DIR)"                                                                                                >> $out/lib/cmake/lvgl/lvgl-config.cmake

      echo "if (lvgl_FOUND AND NOT TARGET lvgl::lvgl)"                                                                                                                                              >> $out/lib/cmake/lvgl/lvgl-config.cmake
      echo "add_library(lvgl::lvgl STATIC IMPORTED)"                                                                                                                                                >> $out/lib/cmake/lvgl/lvgl-config.cmake
      echo "set_target_properties(lvgl::lvgl PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "CXX" IMPORTED_LOCATION "''${lvgl_LIBRARY_DIR}" INTERFACE_INCLUDE_DIRECTORIES "''${lvgl_INCLUDE_DIR}")"   >> $out/lib/cmake/lvgl/lvgl-config.cmake
      echo "endif()"                                                                                                                                                                                >> $out/lib/cmake/lvgl/lvgl-config.cmake

      echo "if (lvgl_FOUND AND NOT TARGET lvgl::demos)"                                                                                                                                             >> $out/lib/cmake/lvgl/lvgl-config.cmake
      echo "add_library(lvgl::demos STATIC IMPORTED)"                                                                                                                                               >> $out/lib/cmake/lvgl/lvgl-config.cmake
      echo "set_target_properties(lvgl::demos PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "CXX" IMPORTED_LOCATION "''${lvgl_LIBRARY_DIR}" INTERFACE_INCLUDE_DIRECTORIES "''${lvgl_INCLUDE_DIR}")"  >> $out/lib/cmake/lvgl/lvgl-config.cmake
      echo "endif()"                                                                                                                                                                                >> $out/lib/cmake/lvgl/lvgl-config.cmake
    '';
  };
in gcc12_multi.stdenv.mkDerivation rec {
  pname = "hhuOS";
  version = "experimental";
  
  src = ./.;

  patches = [
    # Remove use of cmake's FetchContent (not allowed on NixOS as nix expressions have to be pure)
    ./nix-patches/cmake-application-lvgl-CMakeLists.patch
  ];

  # Post/Pre patch shouldn't matter but I use post because the patches are diff'ed from unmodified files, just to be sure
  # Would be nice to abstract all the duplication away but then I want to keep it as readable as possible
  postPatch = ''
    # Patch shebangs
    substituteInPlace ./bios/ovmf/build.sh      --replace "#!/bin/bash" "#!/usr/bin/env bash"
    substituteInPlace ./floppy0/build.sh        --replace "#!/bin/bash" "#!/usr/bin/env bash"
    substituteInPlace ./hdd0/build.sh           --replace "#!/bin/bash" "#!/usr/bin/env bash"
    substituteInPlace ./loader/towboot/build.sh --replace "#!/bin/bash" "#!/usr/bin/env bash"

    # Patch /bin paths, executables used: cp, mv, rm, mkdir, tar (these should all be in /run/current-system/sw/bin/)
    substituteInPlace ./cmake/towboot/CMakeLists.txt    --replace "COMMAND /bin" "COMMAND /run/current-system/sw/bin"
    substituteInPlace ./cmake/music/CMakeLists.txt      --replace "COMMAND /bin" "COMMAND /run/current-system/sw/bin"
    substituteInPlace ./cmake/initrd/CMakeLists.txt     --replace "COMMAND /bin" "COMMAND /run/current-system/sw/bin"
    substituteInPlace ./cmake/grub/CMakeLists.txt       --replace "COMMAND /bin" "COMMAND /run/current-system/sw/bin"
    substituteInPlace ./cmake/floppy0/CMakeLists.txt    --replace "COMMAND /bin" "COMMAND /run/current-system/sw/bin"
    substituteInPlace ./cmake/hdd0/CMakeLists.txt       --replace "COMMAND /bin" "COMMAND /run/current-system/sw/bin"
  '';

  nativeBuildInputs = [ cmake bintools_multi nasm mtools gnutar lvgl ];
  buildInputs = [ qemu ];

  cmakeFlags = [
    "-DCMAKE_C_COMPILER=${gcc12_multi}/bin/gcc"
    "-DCMAKE_CXX_COMPILER=${gcc12_multi}/bin/gcc"
    "-DCMAKE_BUILD_TYPE=Default" # Default/Debug
  ];

  # Done automatically
  # enableParallelBuilding = true;

  meta = with lib; {
    homepage = "https://github.com/churl/hhuos";
    description = ''
      hhuOS - A small operating system
    '';
    licencse = licenses.gpl3Only;
    platforms = platforms.linux;
  };
}
