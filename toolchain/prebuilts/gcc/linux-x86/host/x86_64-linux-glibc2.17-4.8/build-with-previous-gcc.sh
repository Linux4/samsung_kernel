#!/bin/bash -eux

previous_sysroot="glibc2.15-4.8"
mydir="$(dirname "$(readlink -m "$0")")"
cd "${mydir}"

prev_dir="${mydir}/../x86_64-linux-${previous_sysroot}"
if [[ ! -d "${prev_dir}" ]]; then
  echo "${prev_dir} isn't a valid previous home for gcc. Quit" >&2
  exit 1
fi

export CC="${prev_dir}/bin/x86_64-linux-gcc"
export CXX="${prev_dir}/bin/x86_64-linux-g++"

myname="$(basename "${mydir}")"
log_file="/tmp/${myname}-build.log"

note_log_file() {
  echo "NOTE: A full build is also available at ${log_file}"
}

trap note_log_file EXIT

set -o pipefail
./build-raring-multilib-toolchain.sh --verbose |& tee "${log_file}"
