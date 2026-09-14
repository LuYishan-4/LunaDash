# Loaded only by the LuDash terminal profile, after the user's Fish configuration.
# No universal variables, login shell changes, or replacement of ~/.config/fish.
set -g fish_greeting
set -g fish_color_command brcyan
set -g fish_color_param normal
set -g fish_color_error brred
set -g fish_color_autosuggestion 4b5563
set -g fish_color_search_match --background=brblack

function fish_prompt
    set -l last_status $status

    # Accent gradient exported by the built-in terminal; falls back to a calm
    # blue family when the profile is sourced outside of it.
    set -l ac_accent $LUDASH_TERM_AC_ACCENT
    set -l ac_muted $LUDASH_TERM_AC_MUTED
    set -l ac_gray $LUDASH_TERM_AC_GRAY
    if test -z "$ac_accent"
        set ac_accent 9ccbfb
        set ac_muted 7a8390
        set ac_gray 5c636e
    end

    # Left cap + user segment: accent background, bright text.
    set_color $ac_accent
    printf ''
    set_color white --background $ac_accent
    printf '󰣇 %s ' (whoami)

    # Cwd segment: muted background, accent-coloured text.
    set_color $ac_accent --background $ac_muted
    printf ''
    set_color $ac_accent --background $ac_muted
    printf ' %s ' (prompt_pwd)

    # Time segment: grey background, bright text.
    set_color $ac_muted --background $ac_gray
    printf ''
    set_color white --background $ac_gray
    printf ' %s ' (date +%H:%M)

    # Right cap.
    set_color $ac_gray
    printf ''

    set_color normal
    printf '\n'

    if test $last_status -ne 0
        set_color brred
        printf '❯ '
    else
        set_color brcyan
        printf '❯ '
    end
    set_color normal
end
