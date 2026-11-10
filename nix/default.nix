{
  lib,
  stdenv,
  cmake,
  pkg-config,
  hyprlang,
  hyprtoolkit,
  libxkbcommon,
  polkit,
  sdbus-cpp_2,
  systemd,
  version ? "0",
}:
let
  inherit (lib.sources) cleanSource cleanSourceWith;
  inherit (lib.strings) hasSuffix;
in
stdenv.mkDerivation {
  pname = "hyprpolkitagent";
  inherit version;

  src = cleanSourceWith {
    filter =
      name: _type:
      let
        baseName = baseNameOf (toString name);
      in
      !(hasSuffix ".nix" baseName);
    src = cleanSource ../.;
  };

  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  buildInputs = [
    hyprlang
    hyprtoolkit
    libxkbcommon
    polkit
    sdbus-cpp_2
    systemd
  ];

  meta = {
    description = "A polkit authentication agent built with hyprtoolkit";
    homepage = "https://github.com/hyprwm/hyprpolkitagent";
    license = lib.licenses.bsd3;
    maintainers = [ lib.maintainers.fufexan ];
    mainProgram = "hyprpolkitagent";
    platforms = lib.platforms.linux;
  };
}
