{
  lib,
  stdenv,
  nix-filter,
  cmake,
  pkg-config,
  boxed-cpp,
  cairo,
  freetype,
  fontconfig,
  libunicode,
  libutempter,
  termbench-pro,
  qt6,
  boost,
  catch2_3,
  fmt,
  microsoft-gsl,
  range-v3,
  yaml-cpp,
  ncurses,
  file,
  installShellFiles,
  reflection-cpp,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "contour";
  version = "0.6.3-unstable";

  src = nix-filter.lib.filter {
    root = ./..;
    exclude = [
      ".git"
      ".github"
      "target"
      "build"
      (nix-filter.lib.matchExt "nix")
    ];
  };

  # Dependencies are already managed by nix
  cmakeFlags = [ "-DCONTOUR_USE_CPM=OFF" ];

  outputs = [
    "out"
    "terminfo"
  ];

  nativeBuildInputs = [
    cmake
    pkg-config
    ncurses
    file
    qt6.wrapQtAppsHook
    installShellFiles
  ];

  buildInputs = [
    boxed-cpp
    cairo
    fontconfig
    freetype
    libunicode
    termbench-pro
    qt6.qtmultimedia
    qt6.qt5compat
    boost
    catch2_3
    fmt
    microsoft-gsl
    range-v3
    yaml-cpp
    reflection-cpp
  ]
  ++ lib.optionals stdenv.hostPlatform.isLinux [ libutempter ];

  postInstall = ''
    mkdir -p $out/nix-support $terminfo/share
    mv $out/share/terminfo $terminfo/share/
    rm -r $out/share/contour
    echo "$terminfo" >> $out/nix-support/propagated-user-env-packages
  '';

  meta = {
    description = "Modern C++ Terminal Emulator";
    homepage = "https://github.com/contour-terminal/contour";
    license = lib.licenses.asl20;
    platforms = lib.platforms.unix;
    mainProgram = "contour";
  };
})
