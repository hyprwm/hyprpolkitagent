{
  pkgs ? import <nixpkgs> {},
  hyprpolkitagent ? pkgs.callPackage ./default.nix {},
  ...
}: pkgs.mkShell {
  inputsFrom = [ hyprpolkitagent ];
  nativeBuildInputs = [ pkgs.clang-tools ];

  shellHook = let
    inherit (pkgs.lib.strings) concatMapStringsSep;
    qtLibPath = f: concatMapStringsSep ":" f (with pkgs.qt6; [
      qtbase
      qtdeclarative
      qtwayland
      pkgs.hyprland-qt-support
    ]);
  in ''
    # Add Qt-related environment variables.
    export QT_PLUGIN_PATH=${qtLibPath (p: "${p}/lib/qt-6/plugins")}
    export QML2_IMPORT_PATH=${qtLibPath (p: "${p}/lib/qt-6/qml")}

    # Generate compile_commands.json
    CMAKE_EXPORT_COMPILE_COMMANDS=1 cmake -S . -B ./build
    ln -s build/compile_commands.json .
  '';
}
