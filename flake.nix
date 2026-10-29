{
  description = "LunaDash 1.0.2a Wayland desktop";

  inputs.nixpkgs.url = "https://channels.nixos.org/nixpkgs-unstable/nixexprs.tar.zst";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      shellSource = builtins.fetchGit {
        url = "https://github.com/LunaDashWM/LunaDash-Shell.git";
        rev = builtins.substring 0 40 (builtins.readFile ./packaging/shell-revision);
      };
      # SlintLu runs the shell; the package installs this revision as libexec/lunadash/slintlu.
      slintluSource = builtins.fetchGit {
        url = "https://github.com/LunaDashWM/SlintLu.git";
        rev = builtins.substring 0 40 (builtins.readFile ./packaging/slintlu-revision);
      };
      slintSource = builtins.fetchGit {
        url = "https://github.com/slint-ui/slint.git";
        rev = "372cf0ee5577c3dfec309a45e7b778ba4e81b734";
      };
      packageFor = pkgs: pkgs.callPackage ./nix/package.nix {
        revision = self.rev or self.dirtyRev or "unknown";
        inherit shellSource slintSource slintluSource;
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
            packages = [ pkgs.git pkgs.python3 ];
          };
        });
    };
}
