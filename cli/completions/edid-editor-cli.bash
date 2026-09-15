# bash completion for edid-editor-cli                      -*- shell-script -*-
#
# Commands, groups, fields and values come from edid-editor-cli complete,
# which reads them from the file being worked on.

_edid_editor_cli_files()
{
    if declare -F _comp_compgen_filedir >/dev/null; then
        _comp_compgen_filedir
    else
        _filedir
    fi
}

_edid_editor_cli_dequote()
{
    REPLY=$1
    if declare -F _comp_dequote >/dev/null; then
        _comp_dequote "$1" && REPLY=${REPLY[0]}
    elif [[ $1 == \~/* ]]; then
        REPLY=$HOME/${1#\~/}
    fi
}

_edid_editor_cli()
{
    local cur prev words cword comp_args
    if declare -F _comp_initialize >/dev/null; then
        _comp_initialize -n =: -- "$@" || return
    else
        _init_completion -n =: || return
    fi

    case $prev in
        -o | --output)
            _edid_editor_cli_files
            return
            ;;
    esac
    if [[ $cur == -?* ]]; then
        COMPREPLY=($(compgen -W '--output --in-place --ignore-errors
            --edit-read-only --quiet --json --help --version' -- "$cur"))
        return
    fi

    # the words before this one, as the program would get them, without options
    local -a args=()
    local i REPLY
    for ((i = 1; i < cword; i++)); do
        case ${words[i]} in
            -o | --output) ((i++)) ;;
            -?*) ;;
            *)
                _edid_editor_cli_dequote "${words[i]}"
                args+=("$REPLY")
                ;;
        esac
    done
    _edid_editor_cli_dequote "$1"
    local program=$REPLY

    _edid_editor_cli_dequote "$cur"
    local value=$REPLY out
    out=$("$program" complete "${args[@]}" "$value" 2>/dev/null)
    if (($? == 3)); then
        _edid_editor_cli_files
        return
    fi

    # each line is a candidate, then a tab and a description
    local line candidate
    COMPREPLY=()
    while IFS= read -r line; do
        candidate=${line%%$'\t'*}
        [[ $candidate && $candidate == "$value"* ]] && COMPREPLY+=("$candidate")
    done <<<"$out"
    [[ ${COMPREPLY[0]-} == *= ]] && compopt -o nospace

    # the shell puts in only what follows the last = or : it breaks words at
    local breaks=${COMP_WORDBREAKS//[^=:]/}
    if [[ $breaks && $value == *["$breaks"]* ]]; then
        local typed=${value%"${value##*["$breaks"]}"}
        COMPREPLY=("${COMPREPLY[@]#"$typed"}")
    fi
    for i in "${!COMPREPLY[@]}"; do
        printf -v 'COMPREPLY[i]' %q "${COMPREPLY[i]}"
    done
} &&
    complete -F _edid_editor_cli edid-editor-cli
