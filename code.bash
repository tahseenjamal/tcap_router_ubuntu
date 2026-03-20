find . \
  \( -type d \( -name target -o -name .git \) -prune \) -o \
  \( -type f \( -name "*.c" -o -name "*.h" -o -name "*.conf" -o -name "Makefile" \) -print \) \
| while IFS= read -r f; do
    echo "===== $f ====="
    cat "$f"
    echo
done
