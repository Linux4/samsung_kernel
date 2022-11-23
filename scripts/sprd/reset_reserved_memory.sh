#!/usr/bin/env bash

#Function: delete, modify and add the reserved memory module
#delete: Delete directly without any complicated operation
#modify: The starting address of itself and the previous module needs to be modified
#add   : Find a suitable starting address to add

#Variable declaration

#script location
SCRIPT_ABS=$(readlink -f "$0")
#Linux top level directory
declare -r LINUX_TOP_PATH=$(dirname $(dirname $(dirname ${SCRIPT_ABS})))
#Location of the reserved memory configuration file
declare -r RM_CONFIG_PATH="sprd-reserved-memory"
#DTS file location
declare -r ARM64_DTS_PATH="arch/arm64/boot/dts/sprd"
declare -r ARM_DTS_PATH="arch/arm/boot/dts"

#Projects supported by scripts
source "${LINUX_TOP_PATH}/${RM_CONFIG_PATH}/add_board.sh"

declare -r R=_reserved:

#add project
function add_project()
{
    local new_project=$1
    local c
    for c in ${PROJECT__MENU_CHOICES[@]} ; do
        if [ "$new_project" = "$j" ] ; then
            return
        fi
    done
    PROJECT_MENU_CHOICES=(${PROJECT_MENU_CHOICES[@]} $new_project)
}

#print
function print_project_menu()
{
    echo
    echo "You're reset reserved memory"
    echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
    echo "The modified configuration file must be consistent with the selected project name"
    echo "For example:"
    echo -e "\tConfiguration file: sp7731e-1h10.h"
    echo -e "\t     Project      : sp7731e-1h10"
    echo "The modified content will be written to the corresponding dts file, please use \"git diff\" to view"
    echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
    echo
    echo "List project... pick a project:"
    echo

    local i=1
    local choice
    for choice in ${PROJECT_MENU_CHOICES[@]}
    do
        echo "     $i. $choice"
        i=$(($i+1))
    done

    echo
}


#Print list of supported project
function list_project()
{
    local answer

    print_project_menu
    echo -n "Which would you like? [project] "
    read answer

    local selection=

    if [ -z "$answer" ]; then
        selection=sp7731e-1h10
    elif (echo -n $answer | grep -q -e "^[0-9][0-9]*$") ;then
        if [ $answer -le ${#PROJECT_MENU_CHOICES[@]} ]; then
            selection=${PROJECT_MENU_CHOICES[$(($answer-1))]}
        fi
    elif (echo -n $answer); then
        echo "choice string"
        selection=$answer
    fi

    if [ `echo "${selection}" | grep "\-2g"` ]; then
        PROJECT=${selection/\-2g/}
    else
    	PROJECT=${selection}
    fi

    #Where ~ actually means to match the following regular expression.
    #If it matches, it outputs 1 and if it doesn't match, it outputs 0
    if [[ "${PROJECT_ARM64_ARRAY[*]}" =~ ${PROJECT} ]];then
	arch=arm64
    else
	arch=arm
    fi
}

#Add project to the list to be printed
function add_project_array()
{
    local element_arm element_arm64
    for element_arm in ${PROJECT_ARM_ARRAY[@]}
    do
	add_project ${element_arm}
    done

    for element_arm64 in ${PROJECT_ARM64_ARRAY[@]}
    do
	add_project ${element_arm64}
    done

}


#Get the path of DTS corresponding to the selected project
function get_dts_path_file()
{

    local element_arm element_arm64 f s
    for element_arm in ${PROJECT_ARM_ARRAY[@]}
    do
        if [ "$1" == "${element_arm}" ]; then
	    DTS_PATH="${LINUX_TOP_PATH}/${ARM_DTS_PATH}"
        fi
    done

    if [ !${DTS_PATH} ]; then
    	for element_arm64 in ${PROJECT_ARM64_ARRAY[@]}
    	do
            if [ "$1" == "${element_arm64}" ]; then
	    	DTS_PATH="${LINUX_TOP_PATH}/${ARM64_DTS_PATH}"
            fi
    	done
    fi

    #Get the DTS file corresponding to the selected project
    f=`echo "${PROJECT}" | awk -F'-' '{print $1}'`
    s=`echo "${PROJECT}" | awk -F'-' '{print $2}'`

    DTS_FILE[0]=$(ls ${DTS_PATH} | grep $f | grep "mach.dtsi")
    if [ -z "${DTS_FILE[0]}" ]; then
 	echo "$f does not exist!"
        exit
    fi

    DTS_FILE[1]=$(ls ${DTS_PATH} | grep $f | grep $s | grep "\-overlay.dts")
    if [ -z "${DTS_FILE[1]}" ]; then
 	echo "${PROJECT} does not exist!"
        exit
    fi
}


#Get the name, starting address and size of all reserved memory
function get_reserved_memory()
{
    #Number of reserved memories in DTS file marked with m
    #Index indicates the total number of reserved memory in dtsi file
    local i j index

    #Traverse DTS file
    for i in ${DTS_FILE[@]}
    do
	#Save shell built-in array separator
    	OLD_IFS="$IFS"
	#Reset the shell array separator to ","
    	IFS=","
	#Extract common reserved memory

	if [ `echo "$i" | grep "dtsi"` ]; then
            RE=$(grep "_reserved:" -r ${DTS_PATH}/${i} | awk -F'_reserved:' '{print $1}' | awk -F' ' '{print $1}' | awk 'BEGIN{RS="\n";ORS=",";} {print $0}')
            RESERVED=($RE)
	    ST=$(grep "_reserved:" -r ${DTS_PATH}/${i} | awk -F'@' '{print $2}' | awk -F'{' '{print $1}' | awk 'BEGIN{RS="\n";ORS=",";} {print $0}')
	    START=($ST)
            if [ "${arch}" == "arm" ]; then
	        SI=$(grep -A2 "_reserved:" -r ${DTS_PATH}/${i} | grep 'reg = <0x' | awk -F' ' '{print $4}' | awk -F'>' '{print $1}'| awk 'BEGIN{RS="\n";ORS=",";} {print $0}')
            else
	        SI=$(grep -A2 "_reserved:" -r ${DTS_PATH}/${i} | grep 'reg = <0x' | awk -F' ' '{print $6}' | awk -F'>' '{print $1}'| awk 'BEGIN{RS="\n";ORS=",";} {print $0}')
            fi
	    SIZE=($SI)

        #Extract reserved memory of specific project
	elif [ `echo ${i} | grep "overlay.dts"` ]; then
            RE_T=$(grep "_reserved:" -r ${DTS_PATH}/${i} | awk -F'_reserved:' '{print $1}' | awk -F' ' '{print $1}' | awk 'BEGIN{RS="\n";ORS=",";} {print $0}')
	    RESERVED_T=($RE_T)
	    ST_T=$(grep "_reserved:" -r ${DTS_PATH}/${i} | awk -F'@' '{print $2}' | awk -F'{' '{print $1}' | awk 'BEGIN{RS="\n";ORS=",";} {print $0}')
            START_T=($ST_T)
            if [ "${arch}" == "arm" ]; then
	        SI_T=$(grep -A2 "_reserved:" -r ${DTS_PATH}/${i} | grep "reg = <0x" | awk -F' ' '{print $4}' | awk -F'>' '{print $1}' | awk 'BEGIN{RS="\n";ORS=",";} {print $0}')
            else
	        SI_T=$(grep -A2 "_reserved:" -r ${DTS_PATH}/${i} | grep "reg = <0x" | awk -F' ' '{print $6}' | awk -F'>' '{print $1}' | awk 'BEGIN{RS="\n";ORS=",";} {print $0}')
            fi
	    SIZE_T=($SI_T)
        else
	    echo "Reserved memory should not be placed here!"
	fi
	#Restore shell array default separator
	IFS="$OLD_IFS"
    done

    #Used to determine whether the reserved memory module has been redefined
    local flag  n=0
    for j in ${RESERVED_T[@]}
    do
	local m=0
	#The size of the array, that is, the number of reserved memories in dtsi
	index=${#RESERVED[@]}
	flag=NULL
	#Modify the starting address and size of the reserved memory that already exists in the dtsi file
	if [[ "${RESERVED[*]}" =~ ${j} ]]; then
	    flag="one"
	    START[$m]=${START_T[$n]}
            SIZE[$m]=${SIZE_T[$n]}
	    let m+=1
	fi


	if [ "$flag" = "one" ]; then
	    flag=NULL
	    let n+=1
	    continue
	fi


        #Add reserved memory in DTS file to dtsi array
	RESERVED[$index]=${j}
	START[$index]=${START_T[$n]}
	SIZE[$index]=${SIZE_T[$n]}
	#It must be placed here to prevent confusion of reserved memory in traversing DTS files
	let n+=1
        let index+=1
    done
    local v tmp p=0
    for v in ${START[@]}
    do
	if [ `echo "$v" | grep "0x"` ]; then
	    START[$p]=$(echo "$v" | awk -F'0[xX]' '{print $2}')
	fi
	let p+=1
    done
}

#Using bubbling sort to sort reserved memory from small to large address
function address_sort()
{
    #m:Difference between two numbers
    local i j t m
    local len=${#START[@]}

    for((i=0;i<len;i++))
    do
	for((j=0;j<len-i-1;j++))
	do
	    #To improve the readability of the program,
	    #the variable t is used to represent the "START[j+1]element"
	    t=$[$j+1]
	    f="0x${START[$j]}"
            s="0x${START[$t]}"
	    let "m=$f-$s"

	    if [[ $m -gt 0 ]]; then
	        tmp_start=${START[$j]}
		START[$j]=${START[$t]}
		START[$t]=$tmp_start

		tmp_reserved=${RESERVED[$j]}
		RESERVED[$j]=${RESERVED[$t]}
                RESERVED[$t]=$tmp_reserved

		tmp_size=${SIZE[$j]}
		SIZE[$j]=${SIZE[$t]}
		SIZE[$t]=$tmp_size
            fi
	done
    done

}

function reserved_memory_opertion()
{
    #tmp:Add 0x to the element in the start array
    #m:Difference between two numbers
    local element i=0 in name size len_d j tmp m address

    while read line
    do
	if [[ -z ${line} ]]; then
	    continue
	fi

	if [[ ${line} =~ ^'#' ]]; then
	    continue
	fi

	#delete reserved memory
	if [ `echo "$line" | grep "DEL:"` ]; then
	    name=`echo "$line" | awk -F':' '{print $2}'| tr '[A-Z]' '[a-z]'`
	    if [ -z ${name} ];then
		echo "Name is missing. Please check whether it is correctly filled in!"
                exit
	    fi

	    #delete
	    for element in ${DTS_FILE[@]}
	    do
		s=$(grep "${name}${R}" -rn "${DTS_PATH}/${element}" | awk -F':' '{print $1}')
		if [ ${name} = "pstore" ]; then
		    let "e=s+6"
		elif [ ${name} = "iq" ]; then
		    let "e=s+3"
		else
		    let "e=s+2"
		fi
		#delete action
		if [ ${s} ]; then
		    sed -i "${s},${e}d" "${DTS_PATH}/${element}"
		fi

	    done

            #Get the name, size and starting address of the newly generated reserved memory element, and sort it.
	    #When using unset to delete array elements, the index cannot be reordered, so this clumsy method is used.
    	    get_reserved_memory
            address_sort
	elif [[ `echo "$line" | grep "MOD:"` ]]; then
	    name=`echo "$line" | awk -F':' '{print $2}'| awk -F' ' '{print $1}' | tr '[A-Z]' '[a-z]'`
	    size=`echo "$line" | awk -F' ' '{print $2}'| tr '[A-Z]' '[a-z]'`
	    address=`echo "$line" | awk -F' ' '{print $3}'| tr '[A-Z]' '[a-z]'`
	    if [ ! ${name} -o ! ${size} ]; then
		echo "Name or size is missing. Please check whether it is correctly filled in!"
                exit
	    fi

	    if [ -z ${address} ]; then
	    	len_m=${#RESERVED[@]}
	     	#Find the location of the element that needs to be modified
	     	for((j=0;j<len_m;j++))
	     	do
		    if [ "${name}" = "${RESERVED[$j]}" ]; then
		        in=${j}
		        break
		    fi
	     	done

	     	#Calculate the new starting address of the module to be modified
             	mod_start="0x${START[$in]}"
             	let "mod_end=${mod_start}+${SIZE[$in]}"
	     	mod_end="0x$(echo "obase=16;ibase=10; ${mod_end}" | bc)"
	     	let "mod_start=${mod_end}-${size}"
	     	mod_start="$(echo "obase=16;ibase=10; ${mod_start}" | bc)"
             	mod_start=`echo $mod_start |  tr '[A-Z]' '[a-z]'`

		local mod_f mod_1 mod_2
		mod_f=`echo "${mod_start}" | wc -L`
		if [[ ${mod_f} -ge 8 ]]; then
		    let "mod_f-=8"
		    if [[ ${mod_f} -gt 0 ]]; then
		        mod_1=`echo ${mod_start:0:${mod_f}}`
		        mod_2=`echo ${mod_start:0-8:8}`
		    else
		        mod_1="0"
		        mod_2=`echo ${mod_start:0-8:8}`
		    fi
		fi

	     	#Get the line number of the element to be modified from the dts or dtsi file
	     	s1=$(grep "${name}${R}" -rn "${DTS_PATH}/${DTS_FILE[0]}" | awk -F':' '{print $1}')
	     	s2=$(grep "${name}${R}" -rn "${DTS_PATH}/${DTS_FILE[1]}" | awk -F':' '{print $1}')

	     	#This code can be optimized
	     	if [ $name == "pstore" ]; then
                    let "s1_size=${s1}+2"
                    let "s2_size=${s2}+2"
             	elif [[ $name = "iq" ]]; then
		    let "s1_size=${s1}+2"
                    let "s2_size=${s2}+2"
             	else
                    let "s1_size=${s1}+1"
                    let "s2_size=${s2}+1"
             	fi

	     	#Write back the starting address and size of the element to be modified to the array
	     	#START[$in]=$(echo "${new_start}" | awk -F'0x' '{print $2}')
             	#SIZE[$in]=${size}
		if   [[ -n ${s1} && ! -n ${s2} ]]; then
                    sed -i "${s1},${s1} s/@[a-fA-F0-9]*\s*{/@$mod_start {/" "${DTS_PATH}/${DTS_FILE[0]}"
                    if [ "${arch}" == "arm" ]; then
		        sed -i "${s1_size},${s1_size} s/reg = <0x[a-fA-F0-9]* 0x[a-fA-F0-9]*>;/reg = <0x$mod_start $size>;/" "${DTS_PATH}/${DTS_FILE[0]}"
                    else
			if [[ -n ${mod_1} && -n ${mod_2} ]]; then
		            sed -i "${s1_size},${s1_size} s/reg = <0x[a-fA-F0-9]* 0x[a-fA-F0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <$mod_1 0x$mod_2 0x0 $size>;/" "${DTS_PATH}/${DTS_FILE[0]}"
			else
		            sed -i "${s1_size},${s1_size} s/reg = <0x0 0x[a-fA-F0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <0x0 0x$mod_start 0x0 $size>;/" "${DTS_PATH}/${DTS_FILE[0]}"
			fi
                    fi
		elif [ -n ${s2} ]; then
                    sed -i "${s2},${s2} s/@[a-fA-F0-9]*\s*{/@$mod_start {/" "${DTS_PATH}/${DTS_FILE[1]}"
                    if [ "${arch}" == "arm" ]; then
		        sed -i "${s2_size},${s2_size} s/reg = <0x[a-fA-F0-9]* 0x[a-fA-F0-9]*>;/reg = <0x$mod_start $size>;/" "${DTS_PATH}/${DTS_FILE[1]}"
                    else
			if [[ -n ${mod_1} && -n ${mod_2} ]]; then
		            sed -i "${s2_size},${s2_size} s/reg = <0x[a-fA-F0-9]* 0x[a-fA-F0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <0x$mod_1 0x$mod_2 0x0 $size>;/" "${DTS_PATH}/${DTS_FILE[1]}"
			else
		            sed -i "${s2_size},${s2_size} s/reg = <0x0 0x[a-fA-F0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <0x0 0x$mod_start 0x0 $size>;/" "${DTS_PATH}/${DTS_FILE[1]}"
			fi
                    fi
	     	else
		    echo "The reserved memory module to be modified does not exist!"
             	fi

	     	#If the modified element is the last of several consecutive elements,
             	#all elements before it need to be modified
             	for((p=in-1;p>0;p--))
	     	do
		    pre_start="0x${START[$p]}"
		    let "pre_end=${pre_start}+${SIZE[$p]}"
	     	    pre_end="0x$(echo "obase=16;ibase=10; ${pre_end}" | bc)"

                    mod_start="0x${mod_start}"

		    #Modify whether the previous element of the current node needs to be modified
	     	    let "mm=${pre_end}-${mod_start}"
	     	    if [[ ${mm} -gt 0 ]]; then
		        let "pre_start=${mod_start}-${SIZE[$p]}"
	     	        pre_start="$(echo "obase=16;ibase=10; ${pre_start}" | bc)"
             	        pre_start=`echo $pre_start |  tr '[A-Z]' '[a-z]'`

			local pre_f pre_1 pre_2
		        pre_f=`echo "${pre_start}" | wc -L`
		        if [[ ${pre_f} -ge 8 ]]; then
		            let "pre_f-=8"
		            if [[ ${pre_f} -gt 0 ]]; then
		                pre_1=`echo ${pre_start:0:${pre_f}}`
		                pre_2=`echo ${pre_start:0-8:8}`
		            else
		                pre_1="0"
		                pre_2=`echo ${pre_start:0-8:8}`
		        fi
		fi

	            	s3=$(grep "${RESERVED[$p]}${R}" -rn "${DTS_PATH}/${DTS_FILE[0]}" | awk -F':' '{print $1}')
	            	s4=$(grep "${RESERVED[$p]}${R}" -rn "${DTS_PATH}/${DTS_FILE[1]}" | awk -F':' '{print $1}')

	     	    	#This code can be optimized
	     	    	if [ ${RESERVED[$p]} = "pstore" ]; then
                           let s3_size=${s3}+2
                           let s4_size=${s4}+2
                        elif [[ ${RESERVED[$p]} = "iq" ]]; then
		           let s3_size=${s3}+2
                           let s4_size=${s4}+2
                        else
                           let s3_size=${s3}+1
                           let s4_size=${s4}+1
                        fi

		    	#Write back the starting address and size of the element to be modified to the array
		    	#START[$p]=$(echo "${new_start}" | awk -F'0x' '{print $2}')
                    	#SIZE[$p]=${size}
			if [[ -n ${s3} && ! -n ${s4} ]]; then
                            sed -i "${s3},${s3} s/@[a-fA-F0-9]*\s*{/@$pre_start {/" "${DTS_PATH}/${DTS_FILE[0]}"
                            if [ "${arch}" == "arm" ]; then
				sed -i "${s3_size},${s3_size} s/reg = <0x[a-fA-F0-9]* 0x[a-fA-F0-9]*>;/reg = <0x$pre_start ${SIZE[$p]}>;/" "${DTS_PATH}/${DTS_FILE[0]}"
                            else
				if [[ -n ${pre_1} && -n ${pre_2} ]]; then
		                    sed -i "${s3_size},${s3_size} s/reg = <0x[a-fA-F0-9]* 0x[a-fA-F0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <$pre_1 0x$pre_2 0x0 ${SIZE[$p]}>;/" "${DTS_PATH}/${DTS_FILE[0]}"
				else
		                    sed -i "${s3_size},${s3_size} s/reg = <0x0 0x[a-fA-F0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <0x0 0x$pre_start 0x0 ${SIZE[$p]}>;/" "${DTS_PATH}/${DTS_FILE[0]}"
				fi
                            fi
			elif [ -n ${s4} ]; then
                            sed -i "${s4},${s4} s/@[a-fA-F0-9]*\s*{/@$pre_start {/" "${DTS_PATH}/${DTS_FILE[1]}"
                            if [ "${arch}" == "arm" ]; then
		                sed -i "${s4_size},${s4_size} s/reg = <0x[a-fA-F0-9]* 0x[a-fA-F0-9]*>;/reg = <0x$pre_start ${SIZE[$p]}>;/" "${DTS_PATH}/${DTS_FILE[1]}"
                            else
				if [[ -n ${pre_1} && -n ${pre_2} ]]; then
		                    sed -i "${s4_size},${s4_size} s/reg = <0x[a-fA-F0-9]* 0x[a-f0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <0x$pre_1 0x$pre_2 0x0 ${SIZE[$p]}>;/" "${DTS_PATH}/${DTS_FILE[1]}"
				else
		                    sed -i "${s4_size},${s4_size} s/reg = <0x0 0x[a-fA-F0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <0x0 0x$pre_start 0x0 ${SIZE[$p]}>;/" "${DTS_PATH}/${DTS_FILE[1]}"
				fi
                            fi
	            	else
			    echo ""
                    	fi
		    	mod_start=${pre_start}
		     else
		     	break
	            fi
	     	done

             	#Get the name, size and starting address of the newly generated reserved memory element, and sort it.
	     	#When using unset to delete array elements, the index cannot be reordered, so this clumsy method is used.
    	     	get_reserved_memory
               	address_sort
	    else
	     	s5=$(grep "${name}${R}" -rn "${DTS_PATH}/${DTS_FILE[0]}" | awk -F':' '{print $1}')
	     	s6=$(grep "${name}${R}" -rn "${DTS_PATH}/${DTS_FILE[1]}" | awk -F':' '{print $1}')

		local delx add_f add_1 add_2

		delx=`echo "${address/0x/}"`
		add_f=`echo "${delx}" | wc -L`
		if [[ ${add_f} -gt 8 ]]; then
		    let "add_f-=8"
		    add_1=`echo ${delx:0:${add_f}}`
		    add_2=`echo ${delx:0-8:8}`
		fi
		local add_f add_1 add_2
		add_f=`echo "${address}" | wc -L`
		if [[ ${add_f} -ge 8 ]]; then
		    let "add_f-=8"
		    if [[ ${add_f} -gt 0 ]]; then
		        add_1=`echo ${address:0:${add_f}}`
		        add_2=`echo ${address:0-8:8}`
		    else
		        add_1="0"
		        add_2=`echo ${address:0-8:8}`
		fi
		fi

	     	#This code can be optimized
	     	if [ ${RESERVED[$p]} = "pstore" ]; then
                    let "s5_size=${s5}+2"
                    let "s6_size=${s6}+2"
                elif [[ ${RESERVED[$p]} = "iq" ]]; then
		    let "s5_size=${s5}+2"
                    let "s6_size=${s6}+2"
                else
                    let "s5_size=${s5}+1"
                    let "s6_size=${S6}+1"
                fi

		if [[ -n ${s5} && ! -n ${s6} ]]; then
                   if [ "${arch}" == "arm" ]; then
		       sed -i "${s5_size},${s5_size} s/reg = <0x[a-fA-F0-9]* 0x[a-fA-F0-9]*>;/reg = <$address $size>;/" "${DTS_PATH}/${DTS_FILE[0]}"
                   else
		       if [[ -n ${add_1} && -n ${add_2} ]]; then
		           sed -i "${s5_size},${s5_size} s/reg = <0x[a-fA-F0-9]* 0x[a-fA-F0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <$add_1 0x$add_2 0x0 $size>;/" "${DTS_PATH}/${DTS_FILE[0]}"
		       else
		           sed -i "${s5_size},${s5_size} s/reg = <0x0 0x[a-fA-F0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <0x0 $address 0x0 $size>;/" "${DTS_PATH}/${DTS_FILE[0]}"
		       fi
                   fi
		elif [ -n ${s6} ]; then
                   if [ "${arch}" == "arm" ]; then
		       sed -i "${s6_size},${s6_size} s/reg = <0x[a-fA-F0-9]* 0x[a-fA-F0-9]*>;/reg = <$address $size>;/" "${DTS_PATH}/${DTS_FILE[1]}"
                   else
		       if [[ -n ${add_1} && -n ${add_2} ]]; then
		           sed -i "${s6_size},${s6_size} s/reg = <0x[a-fA-F0-9]* 0x[a-fA-F0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <0x$add_1 0x$add_2 0x0 $size>;/" "${DTS_PATH}/${DTS_FILE[1]}"
		       else
		           sed -i "${s6_size},${s6_size} s/reg = <0x0 0x[a-fA-F0-9]* 0x0 0x[a-fA-F0-9]*>;/reg = <0x0 $address 0x0 $size>;/" "${DTS_PATH}/${DTS_FILE[1]}"
		       fi
                   fi
	        else
		   echo ""
                fi
	    fi
	elif [[ `echo "$line" | grep "ADD:"` ]]; then

	     name=`echo "$line" | awk -F':' '{print $2}'| awk -F' ' '{print $1}' | tr '[A-Z]' '[a-z]'`
	     size=`echo "$line" | awk -F':' '{print $2}'| awk -F' ' '{print $2}'`
	     if [ ! ${name} -o ! ${size} ];then
		        echo "Name or size is missing. Please check whether it is correctly filled in!"
                exit
	     fi

             #In order to solve the problem of adding new elements repeatedly in the second execution of the script
	     local exist=0

	     for add in ${RESERVED[@]}
         do
		 if [ ${name} = ${add} ]; then
		    exist=1
                fi
             done

	     len_a=${#RESERVED[@]}

	     if [ ! ${exist} -eq 1 ]; then
	         for((q=len_a-1;q>0;q--))
             	    do
		    local size_t=${size}

		    #pre_end:Before the last element after sorting
	            pre_start="0x${START[$q-1]}"
                    let "pre_end=${pre_start}+${SIZE[$q-1]}"
	     	    pre_end="0x$(echo "obase=16;ibase=10; ${pre_end}" | bc)"

                    #cur_start:Starting address of the last element
                    cur_start="0x${START[$q]}"

                    #The starting address of the last element is subtracted from the ending address
                    #of the previous one to verify that there is enough space to insert the new element
                    let "ma=${cur_start}-${pre_end}"
		    size_t=$(echo "$size_t" | awk -F'0x' '{print $2}')

		    if [[ ${ma} -gt $((16#$size_t)) ]]; then

                        #Get the starting address of the element to insert
		        let "start_add=${cur_start}-${size}"
		        start_add="$(echo "obase=16;ibase=10; ${start_add}" | bc)"
             	        start_add=`echo $start_add |  tr '[A-Z]' '[a-z]'`
		        line_a=$(grep "&reserved_memory {" -rn "${DTS_PATH}/${DTS_FILE[1]}" | awk -F':' '{print $1}')

		        let "t1=$line_a+1"
                        let "t2=$line_a+2"
                        let "t3=$line_a+3"

                        #Add tab in sed: "\ \t"
		        sed -i "${t1}i \ \t${name}${R} ${name}@${start_add} {" "${DTS_PATH}/${DTS_FILE[1]}"
                 	if [ "${arch}" == "arm" ]; then
		            sed -i "${t2}i \ \t \ \treg = <0x$start_add $size>;" "${DTS_PATH}/${DTS_FILE[1]}"
                        else
		            sed -i "${t2}i \ \t \ \treg = <0x0 0x$start_add 0x0 $size>;" "${DTS_PATH}/${DTS_FILE[1]}"
                        fi
		        sed -i "${t3}i \ \t};" "${DTS_PATH}/${DTS_FILE[1]}"
			sed -i "${t3}"G "${DTS_PATH}/${DTS_FILE[1]}"

		        break
		    fi
	         done
	      fi

             #Get the name, size and starting address of the newly generated reserved memory element, and sort it.
	     #When using unset to delete array elements, the index cannot be reordered, so this clumsy method is used.
    	     get_reserved_memory
             address_sort
	else
	    echo "Please modify the keywords in the configuration file as follows:"
	    echo "DEF: TOS"
	    echo "MOD:WB 0x00484000"
            echo "ADD:unisoc 0x00300000"

	fi
    done < ${LINUX_TOP_PATH}/${RM_CONFIG_PATH}/${PROJECT}.cfg

    echo "+++++++++++Reserved memory module after script  execution+++++++++++"
    echo "${RESERVED[*]}"
    echo "${START[*]}"
    echo "${SIZE[*]}"
}

function main()
{
    add_project_array
    list_project
    get_dts_path_file ${PROJECT}
    get_reserved_memory
    address_sort

    echo "+++++++++++Reserved memory module before script execution+++++++++++"
    echo "${RESERVED[*]}"
    echo "${START[*]}"
    echo "${SIZE[*]}"

    reserved_memory_opertion

    echo
    echo "====================================================="
    echo
    echo -e "\tReserved memory has been adjusted!!!"
    echo
    echo "====================================================="

}

main
