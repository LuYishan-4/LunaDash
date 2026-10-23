{
  description = "LunaDash 1.0.1a Wayland desktop";

  inputs.nixpkgs.url = "https://channels.nixos.org/nixpkgs-unstable/nixexprs.tar.zst";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      shellSource = builtins.fetchGit {
        url = "https://github.com/LunaDash-DE/LunaDash-Shell.git";
        rev = builtins.substring 0 40 (builtins.readFile ./packaging/shell-revision);
      };
      packageFor = pkgs: pkgs.callPackage ./nix/package.nix {
        revision = self.rev or self.dirtyRev or "unknown";
        inherit shellSource;
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
        imports = [ ./nix/module.nix ];
        programs.lunadash.package = lib.mkDefault (packageFor pkgs);
      };
      checks = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          package = self.packages.${system}.lunadash;
        in import ./nix/checks.nix {
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
