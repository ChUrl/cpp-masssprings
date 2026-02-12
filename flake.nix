rec {
  description = "C++ ObjRender";

  inputs = {
    nixpkgs.url = "nixpkgs"; # Use nixpkgs from system registry
    flake-utils.url = "github:numtide/flake-utils";

    rust-overlay.url = "github:oxalica/rust-overlay";
    rust-overlay.inputs.nixpkgs.follows = "nixpkgs";

    clj-nix.url = "github:jlesquembre/clj-nix";
    clj-nix.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    rust-overlay,
    clj-nix,
  }:
  # Create a shell (and possibly package) for each possible system, not only x86_64-linux
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs {
        inherit system;
        config.allowUnfree = true;
        overlays = [
          rust-overlay.overlays.default
        ];
      };
      inherit (pkgs) lib stdenv;

      # ===========================================================================================
      # Define custom dependencies
      # ===========================================================================================

      # Python package example
      # typed-ffmpeg = pkgs.python313Packages.buildPythonPackage rec {
      #   pname = "typed_ffmpeg";
      #   version = "3.6";
      #
      #   src = pkgs.python313Packages.fetchPypi {
      #     inherit pname version;
      #     hash = "sha256-YPspq/lqI/jx/9FCQntmQPw4lrPIsdxtHTUg0F0QbrM=";
      #   };
      #
      #   pyproject = true;
      #   build-system = [
      #     pkgs.python313Packages.setuptools
      #     pkgs.python313Packages.setuptools-scm
      #   ];
      # };

      # python = pkgs.python313.withPackages (p:
      #   with p; [
      #     # numpy
      #     # matplotlib
      #     # typed-ffmpeg
      #     # pyside6
      #   ]);

      # rust = pkgs.rust-bin.stable.latest.default.override {
      #   extensions = ["rust-src"]; # Include the Rust stdlib source (for IntelliJ)
      # };

      # 64 bit C/C++ compilers that don't collide (use the same libc)
      bintools = pkgs.wrapBintoolsWith {
        bintools = pkgs.bintools.bintools; # Unwrapped bintools
        libc = pkgs.glibc;
      };
      gcc = lib.hiPrio (pkgs.wrapCCWith {
        cc = pkgs.gcc.cc; # Unwrapped gcc
        libc = pkgs.glibc;
        bintools = bintools;
      });
      clang = pkgs.wrapCCWith {
        cc = pkgs.clang.cc; # Unwrapped clang
        libc = pkgs.glibc;
        bintools = bintools;
      };

      # Multilib C/C++ compilers that don't collide (use the same libc)
      # bintools_multilib = pkgs.wrapBintoolsWith {
      #   bintools = pkgs.bintools.bintools; # Unwrapped bintools
      #   libc = pkgs.glibc_multi;
      # };
      # gcc_multilib = pkgs.hiPrio (pkgs.wrapCCWith {
      #   cc = pkgs.gcc.cc; # Unwrapped gcc
      #   libc = pkgs.glibc_multi;
      #   bintools = bintools_multilib;
      # });
      # clang_multilib = pkgs.wrapCCWith {
      #   cc = pkgs.clang.cc; # Unwrapped clang
      #   libc = pkgs.glibc_multi;
      #   bintools = bintools_multilib;
      # };

      # Raylib CPP wrapper
      raylib-cpp = stdenv.mkDerivation {
        pname = "raylib-cpp";
        version = "5.5.0-unstable-2025-11-12";

        src = pkgs.fetchFromGitHub {
          owner = "RobLoach";
          repo = "raylib-cpp";
          rev = "21b0d0f57a09a7f741d20b7157f440ae87f02c76";
          hash = "sha256-P9x6Zc5t648gR7oYXe38PEX/a4oh4PfuVCnjT0vC10k=";
        };

        # autoPatchelfHook is needed for appendRunpaths
        nativeBuildInputs = with pkgs; [
          cmake
          # autoPatchelfHook
        ];

        buildInputs = with pkgs; [
          raylib
          glfw
          SDL2
        ];

        propagatedBuildInputs = with pkgs; [
          libGLU
          libx11
        ];

        cmakeFlags = [
          "-DBUILD_RAYLIB_CPP_EXAMPLES=OFF"
          "-DBUILD_TESTING=OFF"
          # Point CMake to the nixpkgs raylib so it doesn't try to fetch its own
          "-Draylib_DIR=${pkgs.raylib}/lib/cmake/raylib"
        ];

        # # https://github.com/raysan5/raylib/wiki/CMake-Build-Options
        # cmakeFlags =
        #   [
        #     "-DCUSTOMIZE_BUILD=ON"
        #     "-DPLATFORM=${platform}"
        #   ]
        #   ++ optional (platform == "Desktop") "-DUSE_EXTERNAL_GLFW=ON"
        #   ++ optional includeEverything "-DINCLUDE_EVERYTHING=ON"
        #   ++ optional sharedLib "-DBUILD_SHARED_LIBS=ON";

        # appendRunpaths = optional stdenv.hostPlatform.isLinux (
        #   lib.makeLibraryPath (optional alsaSupport alsa-lib ++ optional pulseSupport libpulseaudio)
        # );

        # passthru.tests = {
        #   inherit raylib-games;
        # };
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
        # python
        # rust
        # bintools
        gcc
        # clang
        # bintools_multilib
        # gcc_multilib
        # clang_multilib
        # clojure
        # jdk

        # C/C++:
        gdb
        # valgrind
        # gnumake
        cmake
        # pkg-config
        # clang-tools

        # Clojure:
        # leiningen
        # clj-nix.packages.${system}.deps-lock

        # Java:
        # gradle

        # Python:
        # hatch
        # py-spy

        # Qt:
        # qt6.wrapQtAppsHook # For the shellHook
      ];

      # Add dependencies to buildInputs if they will end up copied or linked into the final output or otherwise used at runtime:
      # - Libraries used by compilers, for example zlib
      # - Interpreters needed by patchShebangs for scripts which are installed, which can be the case for e.g. perl
      buildInputs = with pkgs; [
        # C/C++:
        # boost
        # sfml
        raylib
        raylib-cpp

        # Qt:
        # qt6.qtbase
        # qt6.full
      ];
      # ===========================================================================================
      # Define buildable + installable packages
      # ===========================================================================================
      # package = stdenv.mkDerivation {
      #   inherit nativeBuildInputs buildInputs;
      #   pname = "";
      #   version = "1.0.0";
      #   src = ./.;
      #
      #   installPhase = ''
      #     mkdir -p $out/bin
      #     mv ./BINARY $out/bin
      #   '';
      # };
      # package = clj-nix.lib.mkCljApp {
      #   inherit pkgs;
      #   modules = [
      #     # Option list: https://jlesquembre.github.io/clj-nix/options/
      #     {
      #       name = "";
      #       version = "1.0.0";
      #       main-ns = "";
      #       projectSrc = ./.;
      #       withLeiningen = true;
      #       buildCommand = "lein uberjar"; # Requires "withLeiningen = true;"
      #       jdk = pkgs.jdk; # Default is pkgs.jdk_headless
      #       # customJdk.enable = true;
      #       # nativeImage.enable = true;
      #     }
      #   ];
      # };
    in rec {
      # Provide package for "nix build"
      # defaultPackage = package;
      # defaultApp = flake-utils.lib.mkApp {
      #   drv = defaultPackage;
      # };

      # Provide environment for "nix develop"
      devShell = pkgs.mkShell {
        inherit nativeBuildInputs buildInputs;
        name = description;

        # =========================================================================================
        # Define environment variables
        # =========================================================================================

        # Rust stdlib source:
        # RUST_SRC_PATH = "${rust}/lib/rustlib/src/rust/library";

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
        # LD_LIBRARY_PATH = nixpkgs.lib.makeLibraryPath buildInputs;

        # QT imports to use with "qmlls -E"
        # QML_IMPORT_PATH = "${pkgs.qt6.full}/lib/qt-6/qml";

        # Set PYTHONPATH
        # PYTHONPATH = ".";

        # Set matplotlib backend
        # MPLBACKEND = "TkAgg";

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

              echo "Removing build directory ./cmake-build-${typeLower}/"
              rm -rf ./cmake-build-${typeLower}

              echo "Creating build directory"
              mkdir cmake-build-${typeLower}
              cd cmake-build-${typeLower}

              echo "Running cmake"
              cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="${type}" -DCMAKE_EXPORT_COMPILE_COMMANDS="On" ..

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
              cmake --build .
            '';

          buildDebug = mkBuildScript "Debug";
          buildRelease = mkBuildScript "Release";

          # Use this to specify commands that should be ran after entering fish shell
          initProjectShell = pkgs.writers.writeFish "init-shell.fish" ''
            echo "Entering \"${description}\" environment..."

            # Determine the project root, used e.g. in cmake scripts
            set -g -x FLAKE_PROJECT_ROOT (git rev-parse --show-toplevel)

            # Rust Bevy:
            # abbr -a build-release-windows "CARGO_FEATURE_PURE=1 cargo xwin build --release --target x86_64-pc-windows-msvc"

            # C/C++:
            # abbr -a cmake-debug "${cmakeDebug}"
            # abbr -a cmake-release "${cmakeRelease}"
            # abbr -a build-debug "${buildDebug}"
            # abbr -a build-release "${buildRelease}"

            # Clojure:
            # abbr -a clojure-deps "deps-lock --lein"

            # Python:
            # abbr -a run "python ./app/main.py"
            # abbr -a profile "py-spy record -o profile.svg -- python ./app/main.py && firefox profile.svg"
            # abbr -a ptop "py-spy top -- python ./app/main.py"
          '';
        in
          builtins.concatStringsSep "\n" [
            # Launch into pure fish shell
            ''
              exec "$(type -p fish)" -C "source ${initProjectShell} && abbr -a menu '${pkgs.bat}/bin/bat "${initProjectShell}"'"
            ''

            # Qt: Launch into wrapped fish shell
            # https://nixos.org/manual/nixpkgs/stable/#sec-language-qt
            # ''
            #   fishdir=$(mktemp -d)
            #   makeWrapper "$(type -p fish)" "$fishdir/fish" "''${qtWrapperArgs[@]}"
            #   exec "$fishdir/fish" -C "source ${initProjectShell} && abbr -a menu '${pkgs.bat}/bin/bat "${initProjectShell}"'"
            # ''
          ];
      };
    });
}
