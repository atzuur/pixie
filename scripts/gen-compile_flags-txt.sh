if [ ! -f "compile_flags.txt" ] || [ "$(cat compile_flags.txt | tr '\n' ' ')" != "$1" ]; then
    echo "updating compile_flags.txt" 1>&2
    echo -n "$(echo "$1" | tr ' ' '\n')" > compile_flags.txt
fi
