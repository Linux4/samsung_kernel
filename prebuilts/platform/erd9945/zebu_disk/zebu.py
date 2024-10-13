#!/usr/bin/env python
#
# Copyright (C) 2020 Samsung Electronics Co., Ltd.
#
# Authors:
#	Kiwoong Kim <kwmad.kim@samsung.com>
#
# This software is proprietary of Samsung Electronics.
# No part of this software, either material or conceptual may be copied or distributed, transmitted,
# transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
# electronic, mechanical, manual or otherwise, or disclosed
# to third parties without the express written permission of Samsung Electronics.

"""zebu storage image builder """

import argparse
import os
import sys
import subprocess
import collections
import xml.etree.ElementTree as ET

import common

"""XML format """
_PART_META = (
  ('p_filesys', 'I'),
  ('p_blknum', 'I'),
  ('p_name', '20s'),
  ('p_fname', '20s'),
)
PartMeta = collections.namedtuple(
  'PartMeta', [_name for _name, _ in _PART_META])

"""global variables """
out_dir = "./"
simg2img_dir = "./simg2img"

pmbr_bin = "pmbr.bin"

gpt_script = 'gpt_zebu'
gpt_bin = "gpt.bin"
sz_sector = 512
head_sz = 512 * 1024
mke2fs_bs = 4096
ofs_sb = 1024

list_paths = []
list_trash = []
list_part = []

def _del_trash():
  """delete temporary files """
  for file in list_trash:
    if os.path.exists(file) and not os.path.isdir(file):
        os.remove(file)
  del list_trash[:]
  print("Delete Intermediate files: PASS\n")

def packed(name, path, size):
  try:
    f = open(path, "ab")
    f.truncate(size)
    f.close()

    size = common.get_size(path)
    list_paths.append(path)
  except Exception as e:
    output = str(e.output).strip()
    sys.exit('{0:s} for {1:s}: ERROR'.format(output, name))
    _del_trash()
  secs = int(size / sz_sector)
  print("--{0:<20s} [ {1:<10d} sectors ] {2:s} for {3:s}: PASS".\
		  format("Create image", secs, path, name))

def _check_files_and_dirs(script, image_path):
  """script """
  if not os.path.exists(script):
    _del_trash()
    sys.exit("No script. Need to set the path by using -s: ERROR")

  """pmbr """
  path = "./" + pmbr_bin
  if not os.path.exists(path):
    _del_trash()
    sys.exit('No {0:s}: ERROR'.format(path))

  """output directory """
  if out_dir != "./":
    if os.path.exists(out_dir):
      if not os.path.isdir(out_dir):
        sys.exit("{0:s} exists and isn't directory. \
			Need to change output directory: ERROR".format(out_dir))
      else:
        print("--{0:s} directory exists".format(out_dir))
    else:
      os.path.mkdir(out_dir)
      if os.path.exists(out_dir):
        print("--{0:s} directory is created ".format(out_dir))
      else:
        _del_trash()
        sys.exit("--{0:s} directory isn't created: ERROR".format(out_dir))

def _parse_script(script):
  """todo: exception, lun """
  file_xml = ET.parse(script)
  root = file_xml.getroot().find('partition')
  path = out_dir + gpt_script
  f = open(path, "w")
  for item in root.iter('item'):
    f.write("0\t" + item.attrib['filesys'] + "\t" + item.attrib['blknum'] \
		    + "\t" + item.attrib['name'] + "\n")
    part = PartMeta(item.attrib['filesys'], item.attrib['blknum'], \
		    item.attrib['name'], item.attrib['fname'])
    list_part.append(part)
  f.close()
  list_trash.append(path)

  """display built gpt layout """
  f = open(path, 'r')
  print('Layout: ')
  while True:
    line = f.readline()
    if not line: break
    stripped_line = line.strip('\n')
    print("{0:s}".format(stripped_line))
  f.close()

  print("Parse xml and build meta: PASS\n")

def _create_non_part_area(dev, layout, bs, gpt_builder):
  """pack with pmbr """
  path = out_dir + 't_' + pmbr_bin
  cmd = 'cp -f ' + "./" + pmbr_bin + ' ' + path
  common.shell(cmd)
  packed("pmbr", path, bs)
  list_trash.append(path)

  """check builder """
  if not os.path.exists(path):
    _del_trash()
    sys.exit('No {0:s}: ERROR'.format(path))

  """run gpt builder """
  path = out_dir + gpt_bin
  cmd = out_dir + gpt_builder + ' -d ' + dev + ' -i ' + layout + ' -o ' + path
  common.shell(cmd)

  """check gpt binary """
  if not os.path.exists(path):
    _del_trash()
    sys.exit('No {0:s}: ERROR'.format(path))

  """pack with gpt """
  packed("gpt", out_dir + gpt_bin, head_sz - bs)

  print("Create and queue non part images: PASS\n")

def _queue_parts(bs, image_path):
  """todo: path """
  for part in list_part:
    filesys = int(part.p_filesys)
    size = int(part.p_blknum) * sz_sector
    t_file = "t_" + part.p_name + ".bin"
    path = out_dir + t_file

    if part.p_fname != "":
      path_i = image_path + part.p_fname
      if not os.path.exists(path_i):
        _del_trash()
        sys.exit("No {0:s}: ERROR".format(path_i))

      """check if .img file is sparse image, if yes, use simg2img"""
      result = subprocess.check_output(['file', path_i])
      result_dec = result.decode('utf-8')
      if "sparse image" in result_dec:
        cmd = simg2img_dir + " " + path_i + " " + path
        common.shell(cmd)
      else:
        common.shell('cp -f ' + path_i + ' '+ path)

    elif filesys >= 2:
      cnt = int(size / mke2fs_bs)

      if filesys == 2:
        fs = "ext4"
      elif filesys == 3:
        fs = "f2fs"
      else:
        fs = "wrong"

      if filesys:
        cmd = "mke2fs -t " + fs + " -b " + str(mke2fs_bs) + " -O uninit_bg -F "\
		+ path + " " + str(cnt)
      print("{0:s}".format(cmd))
      common.shell(cmd)
      """todo: exist """
    else:
      cnt = int(sz_sector / bs)
      cmd = 'dd if=/dev/zero of=' + path + ' bs=' + str(bs) \
	+ ' count=' + str(cnt)
      common.shell(cmd)

    f_size = os.path.getsize(path)
    if f_size > size:
      _del_trash()
      sys.exit("--{0:s}'s size {1:d} is bigger than {2:s}'s part size {3:d}: ERROR".\
			format(t_file, f_size, part.p_name, size))
    packed(part.p_name, path, size)

  print("Create and queue part images: PASS\n")

def _pack_images(path_t):
  """concatenation """
  cmd ='cat '+' '.join(list_paths)+' > '+ path_t
  common.shell(cmd)
  for path in list_paths:
    """print("_pack_images {0:s}".format(path)) """
    list_trash.append(path)

  size = common.get_size(path_t)
  del list_paths[:]
  secs = int(size / sz_sector)
  print("--{0:<20s} [ {1:<10d} sectors], {2:s}: PASS".\
		  format("Concatenate images", secs, path_t))

  print("Pack with images: PASS\n")

def _check_integrity(path, bs):
  magic_num = ''
  """pmbr """
  common.show_hex(0, 512 - 16, 16, path, "pmbr")

  """gpt """
  ret = common.show_hex(bs, 0, 16, path, "gpt")
  if not "EFI" in ret:
    _del_trash()
    sys.exit("Error: [Integrity] gpt packing not correct")

  ofs = 0
  """read only 16 """
  for part in list_part:
    ofs_n = int(part.p_blknum) * sz_sector
    if int(part.p_filesys) == 2:
      """magic for ext4, 0x38 """
      if part.p_name == "super":
        ret = common.show_hex(head_sz + ofs, 4096, 16, path, part.p_name)
        """all zero values in first LBA of super.img as reserved, checking magic number of second LBA. """
        """ LP_METADATA_GEOMETRY_MAGIC 0x616c4467 """
        if not "6744 6c61" in ret:
          _del_trash()
          sys.exit("Error: super: wrong magic number.. please check: {0:d}".format(ret))

      else:
        ret = common.show_hex(head_sz + ofs, ofs_sb + 48, 16, path, part.p_name)
        magic_num = "53ef" # magic number for ext4
        if not magic_num in ret:
          _del_trash()
          sys.exit("Error: {0:s} packing not correct".format(part.p_name))

    elif int(part.p_filesys) == 3:
      """magic for f2fs, 0 """
      common.show_hex(head_sz + ofs, ofs_sb, 16, path, part.p_name)
    elif part.p_name == "vbmeta":
      """check head """
      common.show_hex(head_sz + ofs, 0, 16, path, part.p_name)
    ofs = ofs + ofs_n
  del list_part[:]

  """ show partitions """
  cmd ='gdisk -l ' + path
  print("{0:s}".format(cmd))
  print(common.shell(cmd))

  print("Check integrity: PASS\n")

def _compression(target):
  stem = target.split(".", 1)
  path_g = out_dir + str(stem[0]) + ".gz"
  if os.path.exists(path_g):
    os.remove(path_g)
    if os.path.exists(path_g):
      print('{0:s} exits after removal: ERROR'.format(path_g))
  cmd = "gzip -f " + out_dir + target
  common.shell(cmd)

  print("Compress image, {0:s}: PASS".format(path_g))

def _check_verion_and_system():
  version = sys.version_info[:2]
  if version[0] < 2 or (version[0] == 2 and version[1] < 7):
    _del_trash()
    sys.exit('This script does not support version %d.%d. '
                     'Please use version of 2.7 or newer.\n' % version)

  if (sys.platform.find("linux") < 0):
    _del_trash()
    sys.exit('This script does not support %s. Only for Linux' % sys.platform)

"""todo-list: check path including images, """

def _parse_args():
  parser = argparse.ArgumentParser()

  parser.add_argument("-d", "--dev", type=str, default="mmc",
		  choices=["mmc", "ufs"], help="select device type")
  parser.add_argument("-s", "--script", type=str, default="./gpt_layout_zebu.xml",
                      help="select input xml file")
  parser.add_argument("-i", "--images", type=str, default="./",
                      help="select a directory of source images")
  parser.add_argument("-b", "--builder", type=str, default="./gpt_builder_ver_0_4",
                      help="select a directory of source images")
  '''
  parser.add_argument("-v", "--str_ver", type=int, default=1,
                      help="set structure version")
  '''

  return parser.parse_args()

def main():
  """check version and system """
  _check_verion_and_system()

  """get arguments """
  args = _parse_args()
  dev = args.dev
  target = "zebu_" + dev + ".bin"
  path_target = out_dir + target
  script = args.script
  if not isinstance(args.images, str):
    _del_trash()
    sys.exit("Need to set the path by using -i: {0:s}: ERROR".\
		    format(type(args.images)))

  if args.images[-1] != '/':
    image_path = args.images + "/"
  else :
    image_path = args.images

  gpt_builder = args.builder

  bs = 4096
  if dev == "mmc":
    bs = 512

  """check files and directories """
  _check_files_and_dirs(script, image_path)

  """parse xml and create gpt layout and meta """
  _parse_script(script)

  """create non partition area """
  _create_non_part_area(dev, gpt_script, bs, gpt_builder)

  """queue part images """
  _queue_parts(bs, image_path)

  """pack with part images and build a compressed image """
  _pack_images(path_target)

  """check integrity """
  _check_integrity(path_target, bs)

  """compress image """
  _compression(target)

  """delete intermidiate files """
  _del_trash()
if __name__ == '__main__':
  main()
