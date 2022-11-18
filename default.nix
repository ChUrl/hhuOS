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
    # Change shebangs to #!/usr/bin/env bash instead of #!/bin/bash (doesn't exist on NixOS)
    ./nix-patches/bios-obmf-build.patch
    ./nix-patches/floppy0-build.patch
    ./nix-patches/hdd0-build.patch
    ./nix-patches/loader-towboot-build.patch

    # Remove use of cmake's FetchContent (not allowed on NixOS as nix expressions have to be pure)
    ./nix-patches/cmake-application-lvgl-CMakeLists.patch
  ];

  nativeBuildInputs = [ cmake nasm mtools gnutar lvgl ];
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
