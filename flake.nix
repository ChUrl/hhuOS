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
          overlays = [ devshell.overlays.default ];
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
        clang15_multi = pkgs.wrapCCWith {
          cc = pkgs.clang_15.cc;
          libc = pkgs.glibc_multi;
          bintools = bintools_multi;
        };

        # Used to generate beep files
        hhuOS_python = pkgs.python310.withPackages (p: with p; [
          requests
        ]);

        grub2 = pkgs.hiPrio pkgs.pkgsi686Linux.grub2;

        # Contains the needed lib/grub/i386-efi
        grub2_efi = pkgs.pkgsi686Linux.grub2.override {
          efiSupport = true;
        };
      in {
        # devShell = pkgs.devshell.mkShell ...
        devShell = pkgs.devshell.mkShell {
          name = "hhuOS";

          packages = with pkgs; [
            gcc12_multi
            bintools_multi
            clang15_multi
            clang-tools_15 # clang-format
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
            grub2_efi
            xorriso
            util-linux
            include-what-you-use # Include every used symbol
            cloc # Count lines of code

            # Buildinputs
            qemu # Start os in virtual machine

            # Development
            # jetbrains.clion # Try with global clion
            bear # To generate compilation database
            gdb
            cling # To try out my bullshit implementations
            libsForQt5.umbrello # Code graph explorer
            doxygen # Generate docs + graphs
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
              name = "cpuinfo";
              help = "Show qemu i386 architecture information";
              command = "qemu-system-i386 -cpu help";
            }
          ];
        };
      });
}
