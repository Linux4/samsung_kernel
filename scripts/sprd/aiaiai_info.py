#!/usr/bin/python3

import os
import sys

boardconfig_list=[]
def main():
	result=[]
	for maindir,subdir,file_name_list in os.walk("arch"):
		"""
		print("1:",maindir) #current dir
		print("2:",subdir) # all subdirs
		print("3:",file_name_list)  #all subfiles
		"""
		for filename in file_name_list:
			if filename[:4] == "sprd" and filename[-9:] == "defconfig":
				toolchain=""
				cross_compile=""
				kernel_defconfig=""

				apath = os.path.join(maindir, filename)
				result.append(apath)

				#apath value example: arch/arm64/configs/sprd_sharkl3_defconfig
				arch = (apath.split("/").pop(1))
				if arch == "arm":
					cross_compile= "arm-linux-androidkernel-"
				elif arch == "arm64":
					cross_compile= "aarch64-linux-android-"

				kernel_defconfig = filename
				toolchain="clang"

				boardconfig_list.append(kernel_defconfig+","+arch+","+cross_compile+","+toolchain)

	formatList = list(set(boardconfig_list))
	formatList.sort()

	string_output=""
	for boardconfig in formatList:
		string_output+=boardconfig+" "

	print(string_output)

if __name__ == '__main__':
    main()
