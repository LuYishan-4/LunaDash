const badge = (label: string, color: string, logo: string): string =>
  `https://img.shields.io/badge/${label}-${color}?logo=${logo}&logoColor=white&style=flat-square`;

export const distributions = [
  { name: "Arch Linux", version: "Primary platform", badge: badge("Arch_Linux", "1793D1", "archlinux"), url: "https://archlinux.org", coverage: "Source-build CI", kind: "build", install: "pacman / makepkg" },
  { name: "Ubuntu", version: "24.04", badge: badge("Ubuntu_24.04", "E95420", "ubuntu"), url: "https://ubuntu.com", coverage: "Build + runtime CI", kind: "runtime", install: "apt / CMake" },
  { name: "Debian", version: "13", badge: badge("Debian_13", "A81D33", "debian"), url: "https://www.debian.org", coverage: "Source-build CI", kind: "build", install: "apt / CMake" },
  { name: "Fedora", version: "45", badge: badge("Fedora_45", "51A2DA", "fedora"), url: "https://fedoraproject.org", coverage: "Source-build CI", kind: "build", install: "dnf / CMake" },
  { name: "openSUSE", version: "Tumbleweed", badge: badge("openSUSE_Tumbleweed", "73BA25", "opensuse"), url: "https://www.opensuse.org", coverage: "Source-build CI", kind: "build", install: "zypper / CMake" },
  { name: "Alpine Linux", version: "Edge", badge: badge("Alpine_Edge", "0D597F", "alpinelinux"), url: "https://alpinelinux.org", coverage: "Source-build CI", kind: "build", install: "apk / CMake" },
  { name: "Void Linux", version: "Installer support", badge: badge("Void_Linux", "478061", "voidlinux"), url: "https://voidlinux.org", coverage: "Outside CI matrix", kind: "installer", install: "xbps / CMake" },
  { name: "Gentoo", version: "Installer support", badge: badge("Gentoo", "54487A", "gentoo"), url: "https://www.gentoo.org", coverage: "Outside CI matrix", kind: "installer", install: "emerge / CMake" },
];
