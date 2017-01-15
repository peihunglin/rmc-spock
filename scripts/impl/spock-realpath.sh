# Convert a relative file name to an absolute file name while resolving symbolic links.
spock-realpath() {
    local path="$1"
    local debug=
    [ "$debug" = "" ] || echo "ROBB: spock-realpath '$path'" >&2
    [ "${path#/}" = "$path" ] && path="$(pwd)/$path" # make it absolute

    # Split the name into components
    local old_IFS="$IFS"
    IFS="/"
    set - $path
    IFS="$old_IFS"
    local input=("$@");

    # Process components one at a time to build the output
    local output=()
    while [ "${#input[@]}" -gt 0 ]; do
        [ "$debug" = "" ] || echo "      input = ${input[*]}" >&2
        local in_part="${input[0]}"
        input=("${input[@]:1:${#input[@]}}") # shift first item off array
        [ "$debug" = "" ] || echo "        in_part='$in_part'" >&2

        # Append to the output name
        if [ "$in_part" = "." -o "$in_part" = "" ]; then
            continue
        elif [ "$in_part" = ".." ]; then
            [ ${#output[*]} -gt 0 ] && unset output[${#output[@]}-1]
            continue
        else
            output=("${output[@]}" "$in_part")
        fi

        # Output path
        local output_path="/$(IFS=/; echo "${output[*]}")"
        [ "$debug" = "" ] || echo "        output_path=$output_path" >&2

        # Handle symbolic links
        if [ -h "$output_path" ]; then
            local target_path=$(readlink "$output_path")
            [ "$debug" = "" ] || echo "        is a symbolic link to $target_path" >&2
            old_IFS="$IFS"
            IFS="/"
            set - $target_path
            IFS="$old_IFS"
            local target=("$@")
            [ "$debug" = "" ] || echo "        target parts = ${target[*]}" >&2

            input=("${target[@]}" "${input[@]}")
            if [ "${target_path#/}" = "$target_path" ]; then
                unset output[${#output[@]}-1] # link target is relative
            else
                output=() # link target is absolute
            fi
        fi
    done
    local result=$(IFS=/; echo "/${output[*]}")
    [ "$debug" = "" ] || echo "      result='$result'" >&2
    echo "$result"
}
