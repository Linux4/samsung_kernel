__ZRELADDR     := $(shell /bin/bash -c 'printf "0x%08x" \
	$$[$(TEXT_OFFSET) + 0x80000000]')

   zreladdr-y  := $(__ZRELADDR)

