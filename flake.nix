rec {
  description = "C++ MassSprings";

  inputs = {
    nixpkgs.url = "nixpkgs"; # Use nixpkgs from system registry
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
  # Create a shell (and possibly package) for each possible system, not only x86_64-linux
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
          overlays = [];
        };
        inherit (pkgs) lib stdenv;

        # ===========================================================================================
        # Define custom dependencies
        # ===========================================================================================

        # 64 bit C/C++ compilers that don't collide (use the same libc)
        # bintools = pkgs.wrapBintoolsWith {
        #   bintools = pkgs.bintools.bintools; # Unwrapped bintools
        #   libc = pkgs.glibc;
        # };
        # gcc = lib.hiPrio (pkgs.wrapCCWith {
        #   cc = pkgs.gcc.cc; # Unwrapped gcc
        #   libc = pkgs.glibc;
        #   bintools = bintools;
        # });
        # clang = pkgs.wrapCCWith {
        #   cc = pkgs.clang.cc; # Unwrapped clang
        #   libc = pkgs.glibc;
        #   bintools = bintools;
        # };

        # Raylib CPP wrapper
        # raylib-cpp = stdenv.mkDerivation {
        #   pname = "raylib-cpp";
        #   version = "5.5.0-unstable-2025-11-12";
        #
        #   src = pkgs.fetchFromGitHub {
        #     owner = "RobLoach";
        #     repo = "raylib-cpp";
        #     rev = "21b0d0f57a09a7f741d20b7157f440ae87f02c76";
        #     hash = "sha256-P9x6Zc5t648gR7oYXe38PEX/a4oh4PfuVCnjT0vC10k=";
        #   };
        #
        #   # autoPatchelfHook is needed for appendRunpaths
        #   nativeBuildInputs = with pkgs; [
        #     cmake
        #     # autoPatchelfHook
        #   ];
        #
        #   buildInputs = with pkgs; [
        #     raylib
        #     glfw
        #     SDL2
        #   ];
        #
        #   propagatedBuildInputs = with pkgs; [
        #     libGLU
        #     libx11
        #   ];
        #
        #   cmakeFlags = [
        #     "-DBUILD_RAYLIB_CPP_EXAMPLES=OFF"
        #     "-DBUILD_TESTING=OFF"
        #     # Point CMake to the nixpkgs raylib so it doesn't try to fetch its own
        #     "-Draylib_DIR=${pkgs.raylib}/lib/cmake/raylib"
        #   ];
        # };

        # octree = stdenv.mkDerivation {
        #   pname = "octree";
        #   version = "2.5-unstable-2025-12-18";
        #
        #   src = pkgs.fetchFromGitHub {
        #     owner = "attcs";
        #     repo = "octree";
        #     rev = "5058b3090c8b88e405fe2bfddd6c1c872f2b79d2";
        #     hash = "sha256-a/aDGQ7cj1GbCjts2s9VEaxyFnL6PF+xJOsSxm9o+4M=";
        #   };
        #
        #   # Header-only library
        #   dontBuild = true;
        #   installPhase = ''
        #     mkdir -p $out/include
        #     mv ./*.h $out/include/
        #   '';
        # };

        thread-pool = stdenv.mkDerivation {
          pname = "thread-pool";
          version = "5.1.0";

          src = pkgs.fetchFromGitHub {
            owner = "bshoshany";
            repo = "thread-pool";
            rev = "bd4533f1f70c2b975cbd5769a60d8eaaea1d2233";
            hash = "sha256-/RMo5pe9klgSWmoqBpHMq2lbJsnCxMzhsb3ZPsw3aZw=";
          };

          # Header-only library
          dontBuild = true;
          installPhase = ''
            mkdir -p $out
            mv ./include $out/include
          '';
        };

        # ===========================================================================================
        # Specify dependencies
        # https://nixos.org/manual/nixpkgs/stable/#ssec-stdenv-dependencies-overview
        # Just for a "nix develop" shell, buildInputs can be used for everything.
        # ===========================================================================================

        # Add dependencies to nativeBuildInputs if they are executed during the build:
        # - Those which are needed on $PATH during the build, for example cmake and pkg-config
        # - Setup hooks, for example makeWrapper
        # - Interpreters needed by patchShebangs for build scripts (with the --build flag), which can be the case for e.g. perl
        nativeBuildInputs = with pkgs; [
          # Languages:
          binutils
          gcc

          # C/C++:
          gdb
          valgrind
          # gnumake
          cmake
          # pkg-config
          # clang-tools
          # compdb
          # pprof
          # gprof2dot
          perf
          hotspot
          kdePackages.kcachegrind
          gdbgui
          # renderdoc
        ];

        # Add dependencies to buildInputs if they will end up copied or linked into the final output or otherwise used at runtime:
        # - Libraries used by compilers, for example zlib
        # - Interpreters needed by patchShebangs for scripts which are installed, which can be the case for e.g. perl
        buildInputs = with pkgs; [
          # C/C++:
          # boost
          # sfml
          raylib
          # octree # this one doesn't store center of mass per node - which I need :(
          tracy-wayland
          thread-pool
          backward-cpp
          libbfd
          # llvmPackages.openmp # not required for compilation but for clangd to find the headers
          # raylib-cpp
          # tinyobjloader
          # gperftools
        ];
        # ===========================================================================================
        # Define buildable + installable packages
        # ===========================================================================================
        package = stdenv.mkDerivation rec {
          inherit buildInputs;
          pname = "masssprings";
          version = "1.0.0";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            gcc
            cmake
          ];

          installPhase = ''
            mkdir -p $out/bin
            cp ./${pname} $out/bin/
            cp $src/default.puzzle $out/bin/
          '';
        };
      in rec {
        # Provide package for "nix build"
        defaultPackage = package;
        defaultApp = flake-utils.lib.mkApp {
          drv = defaultPackage;
        };

        # Provide environment for "nix develop"
        devShells = {
          default = pkgs.mkShell {
            inherit nativeBuildInputs buildInputs;
            name = description;

            # =========================================================================================
            # Define environment variables
            # =========================================================================================

            # Custom dynamic libraries:
            # LD_LIBRARY_PATH = builtins.concatStringsSep ":" [
            #   # Rust Bevy GUI app:
            #   # "${pkgs.xorg.libX11}/lib"
            #   # "${pkgs.xorg.libXcursor}/lib"
            #   # "${pkgs.xorg.libXrandr}/lib"
            #   # "${pkgs.xorg.libXi}/lib"
            #   # "${pkgs.libGL}/lib"
            #
            #   # JavaFX app:
            #   # "${pkgs.libGL}/lib"
            #   # "${pkgs.gtk3}/lib"
            #   # "${pkgs.glib.out}/lib"
            #   # "${pkgs.xorg.libXtst}/lib"
            # ];

            # Dynamic libraries from buildinputs:
            LD_LIBRARY_PATH = nixpkgs.lib.makeLibraryPath buildInputs;

            # =========================================================================================
            # Define shell environment
            # =========================================================================================

            # Setup the shell when entering the "nix develop" environment (bash script).
            shellHook = let
              mkCmakeScript = type: let
                typeLower = lib.toLower type;
              in
                pkgs.writers.writeFish "cmake-${typeLower}.fish" ''
                  cd $FLAKE_PROJECT_ROOT

                  # set -g -x CC ${pkgs.clang}/bin/clang
                  # set -g -x CXX ${pkgs.clang}/bin/clang++

                  echo "Removing build directory ./cmake-build-${typeLower}/"
                  rm -rf ./cmake-build-${typeLower}

                  echo "Creating build directory"
                  mkdir cmake-build-${typeLower}
                  cd cmake-build-${typeLower}

                  echo "Running cmake"
                  cmake -G "Unix Makefiles" \
                        -DCMAKE_BUILD_TYPE="${type}" \
                        -DUSE_TRACY=Off \
                        ..

                  echo "Linking compile_commands.json"
                  cd ..
                  ln -sf ./cmake-build-${typeLower}/compile_commands.json ./compile_commands.json
                '';

              cmakeDebug = mkCmakeScript "Debug";
              cmakeRelease = mkCmakeScript "Release";

              mkBuildScript = type: let
                typeLower = lib.toLower type;
              in
                pkgs.writers.writeFish "cmake-build.fish" ''
                  cd $FLAKE_PROJECT_ROOT/cmake-build-${typeLower}

                  echo "Running cmake"
                  NIX_ENFORCE_NO_NATIVE=0 cmake --build . -j$(nproc)
                '';

              buildDebug = mkBuildScript "Debug";
              buildRelease = mkBuildScript "Release";

              # Use this to specify commands that should be ran after entering fish shell
              initProjectShell = pkgs.writers.writeFish "init-shell.fish" ''
                echo "Entering \"${description}\" environment..."

                # Determine the project root, used e.g. in cmake scripts
                set -g -x FLAKE_PROJECT_ROOT (git rev-parse --show-toplevel)

                # C/C++:
                abbr -a cmake-debug "${cmakeDebug}"
                abbr -a cmake-release "${cmakeRelease}"
                abbr -a build-debug "${buildDebug}"
                abbr -a build-release "${buildRelease}"
                abbr -a debug "${buildDebug} && ./cmake-build-debug/masssprings"
                abbr -a release "${buildRelease} && ./cmake-build-release/masssprings"
                abbr -a rungdb "${buildDebug} && gdb --tui ./cmake-build-debug/masssprings"
                abbr -a runtracy "tracy -a 127.0.0.1 &; ${buildRelease} && sudo -E ./cmake-build-release/masssprings"
                abbr -a runvalgrind "${buildDebug} && valgrind --leak-check=full --show-reachable=no --show-leak-kinds=definite,indirect,possible --track-origins=no --suppressions=valgrind.supp --log-file=valgrind.log ./cmake-build-debug/masssprings && cat valgrind.log"
              '';
            in
              builtins.concatStringsSep "\n" [
                # Launch into pure fish shell
                ''
                  exec "$(type -p fish)" -C "source ${initProjectShell} && abbr -a menu '${pkgs.bat}/bin/bat "${initProjectShell}"'"
                ''
              ];
          };

          # TODO: Doesn't work

          # FHS environment for renderdoc. Access with "nix develop .#renderdoc".
          # https://ryantm.github.io/nixpkgs/builders/special/fhs-environments
          # renderdoc =
          #   (pkgs.buildFHSEnv {
          #     name = "renderdoc-env";
          #
          #     targetPkgs = pkgs:
          #       with pkgs; [
          #         # RenderDoc
          #         renderdoc
          #
          #         # Build tools
          #         gcc
          #         cmake
          #
          #         # Raylib
          #         raylib
          #         libGL
          #         mesa
          #
          #         # X11
          #         libx11
          #         libxcursor
          #         libxrandr
          #         libxinerama
          #         libxi
          #         libxext
          #         libxfixes
          #
          #         # Wayland
          #         wayland
          #         wayland-protocols
          #         libxkbcommon
          #       ];
          #
          #     runScript = "fish";
          #
          #     profile = ''
          #     '';
          #   }).env;
        };
      }
    );
}
