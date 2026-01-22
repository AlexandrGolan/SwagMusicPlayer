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
    imagemagick
    taglib
  ];

  buildInputs = with pkgs; [
    glibc
    glibc_multi
    zlib
    imagemagick
    taglib
  ];

  shellHook = ''
    export CC=gcc
    export C_INCLUDE_PATH=${pkgs.glibc.dev}/include:$C_INCLUDE_PATH
    export LIBRARY_PATH=${pkgs.glibc}/lib:$LIBRARY_PATH
    
    if [ ! -f "$(which convert)" ] && [ -f "$(which magick)" ]; then
      echo "Creating convert alias for magick..."
      alias convert='magick'
    fi
  '';
}
