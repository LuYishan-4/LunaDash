# This starter never changes the account's login shell.
if status is-interactive
    if type -q starship
        starship init fish | source
    end
    if type -q zoxide
        zoxide init fish | source
    end
end
if test -f $__fish_config_dir/__custom__.fish
    source $__fish_config_dir/__custom__.fish
end
