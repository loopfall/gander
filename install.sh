#!/bin/bash
# Gander installer for Arch Linux and Omarchy.
#
#   curl -fsSL https://raw.githubusercontent.com/loopfall/gander/main/install.sh | bash
#
# Installs the latest release package with pacman (which also pulls in every
# dependency and replaces Sushi), starts the service, and on Hyprland turns
# off the compositor fade for the preview. Run it again to update.
set -euo pipefail

REPO="loopfall/gander"
ASSET="gander-x86_64.pkg.tar.zst"

bold=$'\e[1m'; dim=$'\e[2m'; green=$'\e[32m'; red=$'\e[31m'; reset=$'\e[0m'
step() { printf '%s==>%s %s\n' "$green$bold" "$reset" "$*"; }
fail() { printf '%sError:%s %s\n' "$red$bold" "$reset" "$*" >&2; exit 1; }

command -v pacman >/dev/null || fail "Gander's installer needs Arch Linux (pacman). On other systems, build from source: see the README."
[ "$(uname -m)" = "x86_64" ] || fail "Prebuilt packages are x86_64 only. Build from source on $(uname -m): see the README."
[ "$EUID" -ne 0 ] || fail "Run this as your normal user, not root. It asks for your password when it needs it."

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

step "Downloading the latest Gander release"
curl -fL --progress-bar -o "$tmp/$ASSET" "https://github.com/$REPO/releases/latest/download/$ASSET" \
  || fail "Download failed. Check your connection, or get the package from https://github.com/$REPO/releases"

step "Installing (pacman asks for your password, and to replace sushi: answer y)"
# Not --noconfirm: replacing sushi is the user's call to make.
sudo pacman -U "$tmp/$ASSET" < /dev/tty

step "Starting the previewer"
systemctl --user daemon-reload
systemctl --user restart gander.service

# Hyprland fades layer surfaces in; Gander animates itself, so the fade is only
# delay. Add the rule once, to whichever config style this machine uses.
if [ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ] || [ -d "$HOME/.config/hypr" ]; then
  lua="$HOME/.config/hypr/looknfeel.lua"
  conf="$HOME/.config/hypr/hyprland.conf"
  if [ -f "$lua" ] && ! grep -q 'namespace = "^gander$"' "$lua"; then
    step "Telling Hyprland not to fade the preview ($lua)"
    printf '\n-- Gander: the preview animates itself; skip the layer fade.\nhl.layer_rule({ match = { namespace = "^gander$" }, no_anim = true, animation = "none" })\n' >> "$lua"
    hyprctl reload >/dev/null 2>&1 || true
  elif [ ! -f "$lua" ] && [ -f "$conf" ] && ! grep -q 'gander' "$conf"; then
    step "Telling Hyprland not to fade the preview ($conf)"
    printf '\n# Gander: the preview animates itself; skip the layer fade.\nlayerrule = noanim, ^(gander)$\n' >> "$conf"
    hyprctl reload >/dev/null 2>&1 || true
  fi
fi

printf '\n%sGander is installed.%s Select a file in Files (Nautilus) and press Space.\n' "$bold" "$reset"
printf '%sFrom a terminal: gander FILE · Keys: press ? in the preview · Uninstall: see the README%s\n' "$dim" "$reset"
