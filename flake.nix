{
  description = "SMP - a small terminal music player for WAV and MP3 files";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: rec {
        smp = pkgs.stdenv.mkDerivation {
          pname = "smp";
          version = "2.0.0";

          src = ./.;

          nativeBuildInputs = with pkgs; [ cmake pkg-config ];
          buildInputs = with pkgs; [ alsa-lib ];

          doCheck = true;

          meta = with pkgs.lib; {
            description = "A small terminal music player";
            license = licenses.mit;
            mainProgram = "smp";
          };
        };

        default = smp;
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            ninja
            gcc
            pkg-config
            alsa-lib
            gdb
            valgrind
            clang-tools
          ];

          shellHook = ''
            echo "SMP dev shell: cmake --preset debug && cmake --build --preset debug"
          '';
        };
      });
    };
}
