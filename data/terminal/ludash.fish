# Loaded only by the LuDash terminal profile, after the user's Fish configuration.
# No universal variables, login shell changes, or replacement of ~/.config/fish.
set -g fish_greeting
set -g fish_color_command brcyan
set -g fish_color_param normal
set -g fish_color_error brred
set -g fish_color_autosuggestion brblack
set -g fish_color_search_match --background=brblack
function fish_prompt
    set -l last_status $status
    set_color brcyan
    printf 'LuDash '
    set_color normal
    printf '%s' (prompt_pwd)
    if test $last_status -ne 0
        set_color brred
        printf ' [%s]' $last_status
    end
    set_color brcyan
    printf '\n❯ '
    set_color normal
end
function fish_right_prompt
    if set -q VIRTUAL_ENV
        set_color brblack
        printf '%s' (basename "$VIRTUAL_ENV")
        set_color normal
    end
end
