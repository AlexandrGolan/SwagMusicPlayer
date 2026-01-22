{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  name = "c-dev-env";

  nativeBuildInputs = with pkgs; [
    gcc
    gnumake
    binutils
    pkg-config
    valgrind
    tiv
    gdb
    alsa-lib
    libsndfile
    mpg123
  ];

  buildInputs = with pkgs; [
    glibc
    glibc_multi
    zlib
  ];

  shellHook = ''
    export CC=gcc
    export C_INCLUDE_PATH=${pkgs.glibc.dev}/include:$C_INCLUDE_PATH
    export LIBRARY_PATH=${pkgs.glibc}/lib:$LIBRARY_PATH
  '';
}
