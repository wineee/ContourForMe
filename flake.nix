{
  description = "A basic flake to help develop contour";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    nix-filter.url = "github:numtide/nix-filter";
  };

  outputs = { self, nixpkgs, flake-utils, nix-filter }@inputs:
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" "riscv64-linux" ]
      (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        rec {
          packages = import ./default.nix {
            inherit pkgs;
            nix-filter = nix-filter;
          };

          devShells.default = pkgs.mkShell {
            inputsFrom = [
              packages.contour
            ];

            packages = with pkgs; [
              ninja
              gdb
            ];

            shellHook =
              let
                makeQtpluginPath = pkgs.lib.makeSearchPathOutput "out" pkgs.qt6.qtbase.qtPluginPrefix;
                makeQmlpluginPath = pkgs.lib.makeSearchPathOutput "out" pkgs.qt6.qtbase.qtQmlPrefix;
              in
              ''
                export QT_PLUGIN_PATH=${makeQtpluginPath (with pkgs.qt6; [ qtbase qtdeclarative qtimageformats qtwayland qtsvg qtmultimedia ])}
                export QML2_IMPORT_PATH=${makeQmlpluginPath (with pkgs.qt6; [ qtdeclarative qtmultimedia qt5compat ])}
                export QML_IMPORT_PATH=$QML2_IMPORT_PATH
              '';
          };
        }
      );
}
