# make project folder & and move cal maps
import os
import sys
import shutil

test_calmap = [
    "# ifdef CONFIG_CAMERA_AAT_V12", 
    "# SYSFS_CFILES += $(SYSFS_PATH)/imgsensor_eeprom_rear_gm2_v007.o", 
    "# SYSFS_CFILES += $(SYSFS_PATH)/imgsensor_eeprom_rear_2p6_v008.o", 
    "# SYSFS_CFILES += $(SYSFS_PATH)/imgsensor_otp_front_sr846d_v008.o", 
    "# SYSFS_CFILES += $(SYSFS_PATH)/imgsensor_otp_front_4ha_v008.o", 
    "# SYSFS_CFILES += $(SYSFS_PATH)/imgsensor_otp_rear2_gc5035b_v008.o", 
    "# SYSFS_CFILES += $(SYSFS_PATH)/imgsensor_otp_rear3_gc02m1b_v008.o", 
    "# SYSFS_CFILES += $(SYSFS_PATH)/imgsensor_eeprom_rear4_gc02m1_v008.o", 
    "# endif"
]


class ProjectFolderMaker():
    def __init__(self, project_name):
        self.project_name = project_name
        self.rom_config_file_c = "imgsensor_vendor_rom_config_{}.c".format(self.project_name)
        self.rom_config_file_h = "imgsensor_vendor_rom_config_{}.h".format(self.project_name)
        self.cal_map_objects = self.get_cal_map_object_list(self.read_makefile())

    def make_project_folder(self):
        os.mkdir(self.project_name)

    def read_makefile(self):
        with open("Makefile") as makefile:
            lines = makefile.readlines()
            return lines

    def replace_object_to_c_and_header(self, objects):
        outputs = []
        for obj in objects:
            outputs.append(obj.replace(".o", ".c"))
            outputs.append(obj.replace(".o", ".h"))
        return outputs

    def get_cal_map_object_list(self, lines):
        config_start = "CONFIG_CAMERA_{}\n".format(self.project_name.upper())
        config_end = "endif\n"
        cal_maps = []
        is_cal_map = False
        for line in lines:
            if is_cal_map:
                if line == config_end:
                    break
                start_file_name = line.find("imgsensor_")
                cal_maps.append(line[start_file_name:].rstrip("\n"))
            if line.find(config_start) > 0:
                is_cal_map = True
        # print("Move {} to {} folder".format(cal_maps, self.project_name))
        print("cal_maps", cal_maps)
        return cal_maps

    def copy_cal_map_files(self):
        cal_maps = self.replace_object_to_c_and_header(self.cal_map_objects)
        for cal_map in cal_maps:
            shutil.copyfile(cal_map, "{}/{}".format(self.project_name, cal_map))

    def move_rom_config_file(self):
        shutil.move(self.rom_config_file_c, os.path.join(self.project_name, self.rom_config_file_c))
        shutil.move(self.rom_config_file_h, os.path.join(self.project_name, self.rom_config_file_h))

    def update_included_header_path(self):
        target_file = "imgsensor_vendor_rom_config.h"
        updated_file = []
        with open(target_file, "r") as file:
            lines = file.readlines()
            for line in lines:
                file_start = line.find(self.rom_config_file_h)
                if file_start > 0:
                    line = line[:file_start] + "./"+ self.project_name + "/" + line[file_start:]
                updated_file.append(line)
        with open(target_file, "w") as file:
            file.writelines(updated_file)
        
    def add_project_makefile(self):
        sysfs_project_path = "SYSFS_PROJECT_PATH := $(IMGSENSOR_COMMON_PATH)/sysfs/{}\n".format(self.project_name)
        rom_config = "SYSFS_CFILES += $(SYSFS_PROJECT_PATH)/{}\n".format(self.rom_config_file_c.replace(".c", ".o"))
        cal_maps = []
        for cal_map in self.cal_map_objects:
            cal_map = "SYSFS_CFILES += $(SYSFS_PROJECT_PATH)/{}\n".format(cal_map)
            cal_maps.append(cal_map)
        with open(os.path.join(self.project_name, "Makefile"), "w") as makefile:
            makefile.write(sysfs_project_path)
            makefile.write("\n")
            makefile.write(rom_config)
            makefile.writelines(cal_maps)




def main(argv):
    project_name = argv[1]
    folder_maker = ProjectFolderMaker(project_name)
    folder_maker.make_project_folder()
    folder_maker.copy_cal_map_files()
    folder_maker.move_rom_config_file()
    folder_maker.add_project_makefile()
    folder_maker.update_included_header_path()


if __name__ == "__main__":
    main(sys.argv)