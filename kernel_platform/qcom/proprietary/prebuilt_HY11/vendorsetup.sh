links_dir="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))"
links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/links"
sys_links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/system_links"
ven_links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/vendor_links"
kernel_links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/kernel_links"
display_links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/display_links"
camera_links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/camera_links"
video_links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/video_links"
sensor_links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/sensors_links"
cv_links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/cv_links"
xr_links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/xr_links"
graphics_links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/graphics_links"
audio_links_path="$(dirname $(readlink -e "${BASH_SOURCE[0]}"))/audio_links"
failed_links=""
count=0
if [[ -f "$links_path" || -f "$sys_links_path" || -f "$ven_links_path" || -f "$kernel_links_path" || -f "$display_links_path" || -f "$camera_links_path" || -f "$video_links_path" || -f "$sensor_links_path" || -f "$xr_links_path" || -f "$graphics_links_path" || -f "$cv_links_path" || -f "$audio_links_path" ]];then
  total_links=$(cat $links_dir/*links| wc -l)
  for i in $(cat $links_dir/*links);do
    src=$(echo $i | awk -F:: '{print $1}')
    dest=$(echo $i | awk -F:: '{print $2}')
    if [ -e "$dest" ];then
      source=$(readlink  -f $dest)
      abs_src=$(readlink  -f $src)
      if [ $source == $abs_src ];then
          continue
      else
          rm -rf $dest
      fi
    fi
    if [[ -e $src && ! -e $dest ]];then
      mkdir -p $(dirname $dest)
      ln -srf $src $dest
      count=$(($count + 1))
    else
      failed_links="$failed_links $i"
    fi
  done
  if [ ! -z "$failed_links" ];then
    echo "*****Could not create symlink*******"
    echo $failed_links | sed 's/[[:space:]]/\n/g'
    echo "****************END******************"
  fi
  echo "Created $count symlinks out of $total_links mapped links.."
fi

# Remove dangling symlinks
if [ ! -d ./vendor/qcom/defs ]; then
    return
fi
symlinks=$(find ./vendor/qcom/defs -type l)
for symlink in $symlinks;do
dest_link=$(readlink -f $symlink)
if [[ ! ( -f $dest_link || -d $dest_link ) ]];then
echo "Removing dangling symlink $symlink"
rm -rf  $symlink
fi
done
