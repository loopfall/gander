-- Optional, for Hyprland: Gander animates its own opening, so the compositor's
-- layer fade only adds delay. Add this line to ~/.config/hypr/looknfeel.lua.
hl.layer_rule({ match = { namespace = "^gander$" }, no_anim = true, animation = "none" })
