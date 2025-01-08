{
  description = "A polkit authentication agent written in QT/QML";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    systems.url = "github:nix-systems/default-linux";

    hyprutils = {
      url = "github:hyprwm/hyprutils";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };

    hyprland-qt-support = {
      url = "github:hyprwm/hyprland-qt-support";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };
  };

  outputs = {
    self,
    nixpkgs,
    systems,
    ...
  } @ inputs: let
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);
    pkgsFor = eachSystem (
      system:
        import nixpkgs {
          localSystem = system;
          overlays = [self.overlays.default];
        }
    );
  in {
    overlays = import ./nix/overlays.nix {inherit inputs self lib;};

    packages = eachSystem (system: {
      default = self.packages.${system}.hyprpolkitagent;
      inherit (pkgsFor.${system}) hyprpolkitagent;
    });

    devShells = eachSystem (system: {
      default = import ./nix/shell.nix {
        pkgs = pkgsFor.${system};
        inherit (pkgsFor.${system}) hyprpolkitagent;
      };
    });
  };
}
