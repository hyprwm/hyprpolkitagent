{
  lib,
  stdenv,
  cmake,
  pkg-config,
  hyprutils,
  hyprland-qt-support,
  kdePackages,
  polkit,
  qt6,
  version ? "0",
}: let
  inherit (lib.sources) cleanSource cleanSourceWith;
  inherit (lib.strings) hasSuffix;
in
  stdenv.mkDerivation {
    pname = "hyprpolkitagent";
    inherit version;

    src = cleanSourceWith {
      filter = name: _type: let
        baseName = baseNameOf (toString name);
      in
        ! (hasSuffix ".nix" baseName);
      src = cleanSource ../.;
    };

    nativeBuildInputs = [
      cmake
      pkg-config
      qt6.wrapQtAppsHook
    ];

    buildInputs = [
      hyprutils
      hyprland-qt-support
      polkit
      kdePackages.polkit-qt-1
      qt6.qtbase
      qt6.qtsvg
      qt6.qtwayland
    ];

    meta = {
      description = "A polkit authentication agent written in QT/QML";
      homepage = "https://github.com/hyprwm/hyprpolkitagent";
      license = lib.licenses.bsd3;
      maintainers = [lib.maintainers.fufexan];
      mainProgram = "hyprpolkitagent";
      platforms = lib.platforms.linux;
    };
  }
