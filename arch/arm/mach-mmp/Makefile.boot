
__ADDRBASE := 0x00000000

__ZRELADDR := $(shell /bin/bash -c 'printf "0x%08x" \
	$$[$(TEXT_OFFSET) + $(__ADDRBASE)]')

zreladdr-y := $(__ZRELADDR)
