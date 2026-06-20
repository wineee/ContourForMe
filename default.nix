{
  pkgs ? import <nixpkgs> { },
  nix-filter ? null,
}:
let
  actualNixFilter = if nix-filter != null then nix-filter else pkgs.nix-filter or (import (builtins.fetchGit {
    url = "https://github.com/numtide/nix-filter.git";
    rev = "319cc83e4492bf22d46e30b3a31c5b8b80b7c7be";
  }));
in
rec {
  contour = pkgs.qt6Packages.callPackage ./nix {
    nix-filter = actualNixFilter;
  };
  default = contour;
}
