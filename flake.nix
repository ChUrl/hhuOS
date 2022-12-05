{
  description = "hhuOS flake for development shell";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.devshell.url = "github:numtide/devshell";

  outputs = { self, nixpkgs, flake-utils, devshell }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true; # For clion
          overlays = [ devshell.overlay ];
        };

        # bintools with multilib
        bintools_multi = pkgs.wrapBintoolsWith {
          bintools = pkgs.bintools.bintools; # Get the unwrapped bintools from the wrapper
          libc = pkgs.glibc_multi;
        };

        # gcc12 with multilib
        gcc12_multi = pkgs.hiPrio (pkgs.wrapCCWith {
          cc = pkgs.gcc12.cc; # Get the unwrapped gcc from the wrapper
          libc = pkgs.glibc_multi;
          bintools = bintools_multi;
        });

        # clang14 with multilib for clang-tools
        clang14_multi = pkgs.wrapCCWith {
          cc = pkgs.clang_14.cc;
          libc = pkgs.glibc_multi;
          bintools = bintools_multi;
        };

        # Used to generate beep files
        hhuOS_python = pkgs.python310.withPackages (p: with p; [
          requests
        ]);
      in {
        # devShell = pkgs.devshell.mkShell ...
        devShell = pkgs.devshell.mkShell {
          name = "hhuOS";

          packages = with pkgs; [
            gcc12_multi
            bintools_multi
            clang14_multi
            hhuOS_python

            # Native buildinputs
            nasm
            cmake
            gnumake
            gnutar # should be in stdenv
            findutils
            dosfstools
            mtools # Generate floppy0.img etc.
            grub2
            xorriso
            util-linux

            # Buildinputs
            qemu # Start os in virtual machine

            # Development
            jetbrains.clion
            bear # To generate compilation database
            gdb
            cling # To try out my bullshit implementations
            # doxygen # Generate docs + graphs
          ];

          # Not for devshell
          # hardeningDisable = [ "fortify" ]; # FORTIFY_SOURCE needs -O2 but we compile with -O0

          commands = [
            {
              name = "ide";
              help = "Run clion for project";
              command = "clion &>/dev/null ./ &";
            }
            {
              name = "clean";
              help = "Remove hhuOS buildfiles";
              command = "./build.sh -c";
            }
            {
              name = "build";
              help = "Build hhuOS using build.sh";
              # NIX_CC required because of nixpkgs bug: https://github.com/NixOS/nixpkgs/pull/192943
              command = "NIX_CC=$(readlink -f $(which gcc)) ./build.sh -g \"Debug\"";
            }
            {
              name = "run";
              help = "Run hhuOS in quemu";
              command = "./run.sh";
            }
            {
              name = "run-gdb";
              help = "Run hhuOS in quemu and wait for gdb connection";
              command = "./run.sh -d 1234";
            }
            {
              name = "connect-gdb";
              help = "Run hhuOS in quemu and wait for gdb connection";
              command = "./run.sh -g 1234";
            }
            {
              name = "cpuinfo";
              help = "Show qemu i386 architecture information";
              command = "qemu-system-i386 -cpu help";
            }
          ];
        };

        defaultPackage = pkgs.callPackage ./default.nix { gcc12_multi=gcc12_multi; bintools_multi=bintools_multi; };
      });
}
