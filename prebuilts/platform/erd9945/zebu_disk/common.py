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

import os
import sys
import subprocess

SZ_SECTOR = 512

def shell(command):
  try:
    output = subprocess.check_output(command, shell=True, stderr=subprocess.STDOUT).strip()
    return output
  except Exception as e:
    output = str(e.output).strip()
    print("{0:s}: ERROR".format(output))
    sys.exit(0)

def show_hex(ofs_s, ofs_c, len, file, name):
  cmd = "xxd -s " + str(ofs_s + ofs_c) + " -l " + str(len) + " " + file
  print("{0:s}:".format(name))
  ret = shell(cmd)
  ret_dec = ret.decode('utf-8')
  print("{0:s}:".format(ret_dec))
  return ret_dec

def get_size(path):
  if os.path.exists(path):
    size = os.path.getsize(path)
    if size == 0:
      sys.exit("--{0:s}'s size is zero: ERROR".format(path))
  else:
    sys.exit("--{0:s} doesn't exists: ERROR".format(path))
  return size

def get_truncated_size(len):
  len = ((len + SZ_SECTOR - 1) / SZ_SECTOR) * SZ_SECTOR
  return len

def check_files_and_dirs(out_dir, files):
  """output directories """
  if out_dir != "./":
    if os.path.exists(out_dir):
      if not os.path.isdir(out_dir):
        sys.exit("{0:s} exists and isn't directory. \
	  Need to change directory: ERROR".format(out_dir))
      else:
        """print("--{0:s} directory exists".format(out_dir)) """
    else:
      os.path.mkdir(out_dir)
      if os.path.exists(out_dir):
        print("--{0:s} directory is created ".format(out_dir))
      else:
        sys.exit("--{0:s} directory isn't created: ERROR".format(out_dir))

  """files """
  for f in files:
    if not os.path.exists(f):
      sys.exit("No input file. Need to check by using -h: {0:s}: ERROR".format(f))

  print("Check output directory and input file existence: PASS\n")

def check_verion_and_system():
  version = sys.version_info[:2]

  if version[0] < 2 or (version[0] == 2 and version[1] < 7):
    sys.exit('This script does not support version %d.%d. '
                     'Please use version of 2.7 or newer.\n' % version)

  if (sys.platform.find("linux") < 0):
    sys.exit('This script does not support %s. Only for Linux' % sys.platform)

def delete_trash(list_t):
  for file in list_t:
    os.remove(file)
    print("---delete {0:s}".format(file))
  del list_t[:]
  print("Delete intermidiate files: PASS\n")

def delete_output(path):
  if os.path.isfile(path):
    os.remove(path)
