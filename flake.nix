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

        windowsPkgs = import nixpkgs {
          inherit system;
          crossSystem = {
            config = "x86_64-w64-mingw32";
          };
          config.allowUnfree = true;
          overlays = [];
        };

        # ===========================================================================================
        # Define custom dependencies
        # ===========================================================================================

        raygui = stdenv.mkDerivation (finalAttrs: {
          pname = "raygui";
          version = "4.0-unstable-2026-02-24";

          src = pkgs.fetchFromGitHub {
            owner = "raysan5";
            repo = "raygui";
            rev = "5788707b6b7000343c14653b1ad3b971ca0597e4";
            hash = "sha256-wKylPeNw7wO5xuTfnp1OYETQ78EPlr4NU9erbmIFgjE=";
          };

          patches = [./raygui.patch];

          dontBuild = true;
          installPhase = ''
            runHook preInstall

            mkdir -p $out/{include,lib/pkgconfig}

            install -Dm644 src/raygui.h $out/include/raygui.h

            cat <<EOF > $out/lib/pkgconfig/raygui.pc
            prefix=$out
            includedir=$out/include

            Name: raygui
            Description: Simple and easy-to-use immediate-mode gui library
            URL: https://github.com/raysan5/raygui
            Version: ${finalAttrs.version}
            Cflags: -I"{includedir}"
            EOF

            runHook postInstall
          '';
        });

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
          ninja
          # cling
          # pkg-config
          # clang-tools
          # compdb
          # pprof
          # gprof2dot
          perf
          hotspot
          kdePackages.kcachegrind
          gdbgui
          massif-visualizer
          heaptrack
          # renderdoc
        ];

        # Add dependencies to buildInputs if they will end up copied or linked into the final output or otherwise used at runtime:
        # - Libraries used by compilers, for example zlib
        # - Interpreters needed by patchShebangs for scripts which are installed, which can be the case for e.g. perl
        buildInputs = with pkgs; [
          # C/C++:
          raylib
          raygui
          thread-pool
          boost

          # Debugging
          tracy-wayland
          backward-cpp
          libbfd
          catch2_3
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

          cmakeFlags = [
            "-DDISABLE_TRACY=On"
            "-DDISABLE_BACKWARD=On"
            "-DDISABLE_TESTS=On"
          ];

          hardeningDisable = ["all"];

          preConfigure = ''
            unset NIX_ENFORCE_NO_NATIVE
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp ./${pname} $out/bin/
            cp $src/default.puzzle $out/bin/
            cp -r $src/fonts $out/bin/fonts
            cp -r $src/shader $out/bin/shader
          '';
        };

        windowsPackage = windowsPkgs.stdenv.mkDerivation rec {
          pname = "masssprings";
          version = "1.0.0";
          src = ./.;

          # nativeBuildInputs must be from the build-platform (not cross)
          # so we use "pkgs" here, not "windowsPkgs"
          nativeBuildInputs = with pkgs; [
            cmake
          ];

          buildInputs = with windowsPkgs; [
            raylib
            raygui
            thread-pool

            # Disable stacktrace since that's platform dependant and won't cross compile to windows
            # https://github.com/NixOS/nixpkgs/blob/master/pkgs/development/libraries/boost/generic.nix#L43
            (boost.override {
              enableShared = false;
              extraB2Args = [
                "--without-stacktrace"
              ];
            })
          ];

          cmakeFlags = [
            "-DCMAKE_SYSTEM_NAME=Windows"
            "-DDISABLE_TRACY=On"
            "-DDISABLE_BACKWARD=On"
          ];

          installPhase = ''
            mkdir -p $out/bin
            cp ./${pname}.exe $out/bin/
            cp $src/default.puzzle $out/bin/
            cp -r $src/fonts $out/bin/fonts
            cp -r $src/shader $out/bin/shader
          '';
        };
      in rec {
        # Provide packages for "nix build" and "nix run"
        packages = {
          default = package;
          windows = windowsPackage;
        };
        apps.default = flake-utils.lib.mkApp {drv = package;};

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
                  cmake -G "Ninja" \
                        -DCMAKE_BUILD_TYPE="${type}" \
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
                abbr -a debug-clean "${cmakeDebug} && ${buildDebug} && ./cmake-build-debug/masssprings"
                abbr -a release-clean "${cmakeRelease} && ${buildRelease} && ./cmake-build-release/masssprings"

                abbr -a rungdb "${buildDebug} && gdb --tui ./cmake-build-debug/masssprings"
                abbr -a runperf "${buildRelease} && perf record -g ./cmake-build-release/masssprings && hotspot ./perf.data"
                abbr -a runtracy "tracy -a 127.0.0.1 &; ${buildRelease} && sudo -E ./cmake-build-release/masssprings"
                abbr -a runvalgrind "${buildDebug} && valgrind --leak-check=full --show-reachable=no --show-leak-kinds=definite,indirect,possible --track-origins=no --suppressions=valgrind.supp --log-file=valgrind.log ./cmake-build-debug/masssprings && cat valgrind.log"
                abbr -a runtests "${buildDebug} && ./cmake-build-debug/tests"

                abbr -a runclion "clion ./CMakeLists.txt 2>/dev/null 1>&2 & disown;"
              '';
            in
              builtins.concatStringsSep "\n" [
                # Launch into pure fish shell
                ''
                  exec "$(type -p fish)" -C "source ${initProjectShell} && abbr -a menu '${pkgs.bat}/bin/bat "${initProjectShell}"'"
                ''
              ];
          };

          # TODO: Can't get renderdoc in FHS to work

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