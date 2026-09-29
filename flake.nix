{
  description = "LunaDash 1.0.1a Wayland desktop";

  inputs.nixpkgs.url = "https://channels.nixos.org/nixpkgs-unstable/nixexprs.tar.zst";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      packageFor = pkgs: pkgs.callPackage ./packaging/nixos/package.nix {
        revision = self.rev or self.dirtyRev or "unknown";
      };
    in {
      overlays.default = final: prev: { lunadash = packageFor final; };
      packages = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in rec {
          lunadash = packageFor pkgs;
          default = lunadash;
        });
      nixosModules.default = self.nixosModules.lunadash;
      nixosModules.lunadash = { pkgs, lib, ... }: {
        imports = [ ./packaging/nixos/module.nix ];
        programs.lunadash.package = lib.mkDefault (packageFor pkgs);
      };
      checks = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          package = self.packages.${system}.lunadash;
        in import ./packaging/nixos/checks.nix {
          inherit pkgs package system nixpkgs;
          module = self.nixosModules.lunadash;
        });
      devShells = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.lunadash ];
            packages = [ pkgs.git pkgs.python3 pkgs.qt6.qtshadertools ];
          };
        });
    };
}
