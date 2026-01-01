{
  description = "NixOS driver for Novation Launchpad (NVLPD01)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          # Helper to get the current kernel
          kernel = pkgs.linuxPackages.kernel;
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "novalpdrv";
            version = "2.0";
            src = ./.;

            nativeBuildInputs = kernel.moduleBuildDependencies;

            buildPhase = ''
              # We point 'make' at the kernel source, but tell it 
              # to build the module located in the current directory (M=$PWD)
              make -C ${kernel.dev}/lib/modules/${kernel.modDirVersion}/build \
                M=$(pwd) \
                modules
            '';

            installPhase = ''
              mkdir -p $out/lib/modules/${kernel.modDirVersion}/misc
              cp novalpdrv.ko $out/lib/modules/${kernel.modDirVersion}/misc/
            '';
          };
        });

      nixosModules.default = { config, pkgs, ... }: {
        # This ensures the module is built against the system's actual kernel version
        boot.extraModulePackages = [ self.packages.${pkgs.system}.default ];
        boot.kernelModules = [ "novalpdrv" ];
        
        services.udev.extraRules = ''
          KERNEL=="nlp*", MODE="0666", GROUP="users"
          SUBSYSTEM=="usb", ATTRS{idVendor}=="1235", ATTRS{idProduct}=="000e", MODE="0666", GROUP="users"
        '';
      };
    };
}
