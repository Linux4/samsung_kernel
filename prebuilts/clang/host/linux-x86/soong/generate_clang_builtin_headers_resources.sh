#!/bin/bash
# This script expects that its first argument is the path prefix and the
# remaining ones are file paths. It generates a C source code containing
# the definition of the array of the 'FileToc' items. FileToc is a struct
# with two strings. The first is the file path with path prefix removed,
# and the second is the file contents as string. The last array element
# is a sentinel (both strings are null). One of the Kythe applications
# relies on this code (Kythe is the source code indexer: http://kythe.io/)

set -eu
declare prefix="$1"
shift
printf 'static const struct FileToc kPackedFiles[] = {\n'
[[ "$prefix" =~ /\/$/ ]] || prefix="$prefix/"
for hfile in "$@"; do
  printf '{"%s",\nR"filecontent(' "${hfile#$prefix}"
  cat "$hfile"
  printf ')filecontent"\n},\n'
done
printf '{nullptr, nullptr}};\n'
