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
		zlib
	];

	shellHook = ''
		export CC=gcc
		export C_INCLUDE_PATH=${pkgs.glibc.dev}/inlcude:$C_INCLUDE_PATH
	'';
}

