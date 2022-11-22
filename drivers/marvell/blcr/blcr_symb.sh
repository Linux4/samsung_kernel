#!/bin/bash

kernelsym=$1

if [ ! -f 'blcr_symb' ] || [ ! -f "$kernelsym" ];
then
	echo "no blcr_symb or no System.map"
	exit 1
fi

prefix="CR_NONE"

echo "Parse kernel symbol ..."
rm -f blcr_config.h
date=`date`
echo "#define BLCR_CONFIG_TIMESTAMP \"$date\"" > blcr_config.h
echo "#ifdef __KERNEL__" >> blcr_config.h

rm -f blcr_imports/import_check.c
cat > blcr_imports/import_check.c <<-EOF
#define KCODE_CHECK(xname) \
    if (CR_KCODE_##xname != kallsyms_lookup_name(#xname)) \
        return -EINVAL;

#define KDATA_CHECK(xname) \
    if (CR_KDATA_##xname != kallsyms_lookup_name(#xname)) \
        return -EINVAL;
EOF

rm -f blcr_imports/imports.c
cat > blcr_imports/imports.c <<-EOF

#ifndef EXPORT_SYMTAB
  #define EXPORT_SYMTAB
#endif

#include "blcr_config.h"
#include "blcr_imports.h"
#include "blcr_ksyms.h"

EOF

match="NO"
while read line
do
	if [ "$(echo $line | grep '\<section\>')" != "" ];
	then
		prefix=`echo ${line} | cut -d' ' -f2`
		if [ "$match" = "YES" ];
		then
			echo "ERROR: section doesn't match"
			exit 1;
		fi
		match="YES"
		continue
	fi
	if [ "$(echo $line | grep -e '\<end\>')" != "" -a "$match" = "YES" ];
	then
		prefix="CR_NONE"
		match="NO"
		continue
	fi
	if [ "${line}" == "" ];
	then
		continue
	fi
	if [  "$match" != "YES" ];
	then
		echo "ERROR: section doesn't match"
		exit 1;
	fi
	name=$line
	addr=`cat $kernelsym | grep -e "\<${name}\>" | cut -d' ' -f1`
	if [ "${addr}" == "" ];
	then
		echo "ERROR: no address for $name"
		exit 1
	fi

	#echo "#define CR_${prefix}_${name} 0x${addr}"
	echo "#define CR_${prefix}_${name} 0x${addr}" >> blcr_config.h
	if [ ${prefix} = "KCODE" -o  ${prefix} = "KDATA" ];
	then
		echo "_CR_IMPORT_${prefix}(${name}, 0x${addr})" >> blcr_imports/imports.c
	fi
	if [ ${prefix} = "KCODE" ];
	then
		echo "KCODE_CHECK(${name})" >> blcr_imports/import_check.c
	fi
	if [ ${prefix} = "KDATA" ];
	then
		echo "KDATA_CHECK(${name})" >> blcr_imports/import_check.c
	fi

done < blcr_symb

if [  "$match" != "NO" ];
then
	echo "ERROR: section doesn't match"
	exit 1;
fi

echo "#endif //__KERNEL__" >> blcr_config.h
cat blcr_kcon >> blcr_config.h

cat >> blcr_imports/imports.c <<-EOF
/* END AUTO-GENERATED FRAGMENT */

const char *blcr_config_timestamp = BLCR_CONFIG_TIMESTAMP;
EXPORT_SYMBOL_GPL(blcr_config_timestamp);
EOF

echo "done."
