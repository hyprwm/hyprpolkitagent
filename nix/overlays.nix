{
  inputs,
  self,
  lib,
}:
let
  mkDate =
    longDate:
    (lib.concatStringsSep "-" [
      (builtins.substring 0 4 longDate)
      (builtins.substring 4 2 longDate)
      (builtins.substring 6 2 longDate)
    ]);
  date = mkDate (self.lastModifiedDate or "19700101");
  version = lib.removeSuffix "\n" (builtins.readFile ../VERSION);
in
{
  default = self.overlays.hyprpolkitagent;

  hyprpolkitagent-with-deps = lib.composeManyExtensions [
    inputs.hyprutils.overlays.default
    inputs.hyprlang.overlays.default
    inputs.hyprgraphics.overlays.default
    inputs.aquamarine.overlays.default
    inputs.hyprtoolkit.overlays.default
    self.overlays.hyprpolkitagent
  ];

  hyprpolkitagent = final: prev: {
    hyprpolkitagent = final.callPackage ./. {
      stdenv = final.gcc16Stdenv;
      version = "${version}+date=${date}_${self.shortRev or "dirty"}";
    };
  };
}
