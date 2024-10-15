#!/vendor/bin/sh

#########################################################
### init.insmod.cfg format:                           ###
### ------------------------------------------------  ###
### [insmod|setprop|enable/modprobe] [path|prop name] ###
### ...                                               ###
#########################################################

function get_module_dir()
{
  local type=$1
  modules_dir=
  for f in /${type}/lib/modules/*/modules.dep /${type}/lib/modules/modules.dep /${type}_dlkm/lib/modules/*/modules.dep; do
    if [[ -f "$f" ]]; then
      modules_dir="$(dirname "$f")"
      break
    fi
  done
  echo "${modules_dir}"
}

if [ $# -eq 1 ]; then
  cfg_file=$1
else
  # Set property even if there is no insmod config
  # to unblock early-boot trigger
  setprop vendor.common.modules.ready
  setprop vendor.device.modules.ready
  exit 1
fi

if [ "$cfg_file" == "system_dlkm" ]; then
  modules_dir=$(get_module_dir system)
  if [[ -z "${modules_dir}" ]]; then
    echo "Unable to locate kernel modules directory" 2>&1
    exit 1
  fi
  echo "modules_dir is ${modules_dir}" 2>&1
  modprobe -s -a -d "${modules_dir}" --all=${modules_dir}/modules.load
  if [ $? -eq 0 ]; then
    setprop vendor.system_dlkm.modules.ready true
  fi
elif [ "$cfg_file" == "vendor_dlkm" ]; then
  modules_dir=$(get_module_dir vendor)
  if [[ -z "${modules_dir}" ]]; then
    echo "Unable to locate kernel modules directory" 2>&1
    exit 1
  fi
  echo "modules_dir is ${modules_dir}" 2>&1
  modprobe -s -a -d "${modules_dir}" --all=${modules_dir}/modules.load
  if [ $? -eq 0 ]; then
    setprop vendor.vendor_dlkm.modules.ready true
  else
    echo "modprobe failed! Loading modules using insmod"
    vendor_dlkm_cfg="/vendor/etc/init.insmod.vendor_dlkm.cfg"
    if [ ! -f $vendor_dlkm_cfg ]; then
      echo "$vendor_dlkm_cfg not found!"
      exit 1
    fi
    for m in $(cat ${vendor_dlkm_cfg}); do
      insmod ${modules_dir}/$(basename $m)
    done
  fi
elif [ "$cfg_file" == "wifi" ]; then
  vendor_modules_dir=$(get_module_dir vendor)
  system_modules_dir=$(get_module_dir system)
  
  modules_load_file="$system_modules_dir/modules.load"
  rfkill_relative_filepath=$(grep rfkill $modules_load_file)
  rfkill_absolute_filepath="$system_modules_dir/$rfkill_relative_filepath"

  insmod ${rfkill_absolute_filepath}
  insmod ${vendor_modules_dir}/cfg80211.ko
  insmod ${vendor_modules_dir}/kiwi_v2.ko
elif [ -f $cfg_file ]; then
  modules_dir=$(get_module_dir vendor)
  while IFS="|" read -r action arg
  do
    case $action in
      "insmod") insmod $arg ;;
      "setprop") setprop $arg 1 ;;
      "enable") echo 1 > $arg ;;
      "modprobe") modprobe -s -a -d "${modules_dir}" $arg ;;
    esac
  done < $cfg_file
fi
