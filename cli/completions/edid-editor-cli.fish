# fish completion for edid-editor-cli
#
# Commands, groups, fields and values come from edid-editor-cli complete,
# which reads them from the file being worked on.

function __edid_editor_cli_candidates
    set -l tokens (commandline -xpc 2>/dev/null; or commandline -opc)
    set -l current (commandline -ct | string unescape)
    set -l words
    set -l skip 0
    for token in $tokens[2..-1]
        if test $skip = 1
            set skip 0
            continue
        end
        switch $token
            case -o --output
                set skip 1
            case -
                set -a words $token
            case '-*'
            case '*'
                set -a words $token
        end
    end
    $tokens[1] complete $words "$current" 2>/dev/null
    if test $status -eq 3
        __fish_complete_path "$current"
    end
end

complete -c edid-editor-cli -f -a '(__edid_editor_cli_candidates)'
complete -c edid-editor-cli -s o -l output -r -F -d 'Write a changed EDID to this file'
complete -c edid-editor-cli -s i -l in-place -d 'Write a changed EDID back to the file'
complete -c edid-editor-cli -l ignore-errors -d 'Open EDID data that breaks the standard'
complete -c edid-editor-cli -l edit-read-only -d 'Allow writing fields derived from other data'
complete -c edid-editor-cli -s q -l quiet -d 'Leave out notices about the data'
complete -c edid-editor-cli -l json -d 'Print as JSON'
complete -c edid-editor-cli -s h -l help -d 'Show the help'
complete -c edid-editor-cli -l version -d 'Show the version'
