import argparse
import os
import re
import shutil
import sys
from lib import dainfo
from lib.sign_util import SLOG_D
from lib.sign_util import SLOG_I
from lib.sign_util import SLOG_E
from lib.sign_util import remove_file

from lib.sign_error import CODE_SUCCESS

def print_usage():
    print ("python resign_da.py [in_da_file_path] [chip_name] [bbchip_config_path] [load_region_idx] [out_da_file_path]")
    print ("optional: sec_level=[sec_level] root_key_ver=[root_key_ver] type=[legacy or cert_chain]")
    print ("example to sign all load regions in MT6755:")
    print ("python resign_da.py in/da.bin MT6755 bbchips.ini all out/da-resign.bin")
    print ("example to sign only load region 0 in MT6755:")
    print ("python resign_da.py in/da.bin MT6755 bbchips.ini 0 out/da-resign.bin")
    print ("note: da.bin may contain multiple das, one for one chip")


def fill_arg_dict(input_string, key, args):
    """
    Fill up argument dictionary from input parameters
    """
    prefix = input_string.split("=")[0].strip()
    fmt = re.compile(key, re.I)
    if fmt.search(prefix):
        if input_string.find("root") != -1 and key == "ver":
            return args
        val = input_string.split("=")[1].strip()
        args[key] = val
    return args


def parse_arg(argv):
    """
    Parse input arguments and save the result into argument dictionary.
    """
    args = {'in_path': '', \
            'chip': '', \
            'config_path': '', \
            'load_region_idx': '', \
            'out_path': '', \
            'sec_level': '0', \
            'root_key_ver': '0', \
            'type': 'legacy'}
    for input_string in argv:
        if input_string == "resign_da.py":
              continue
        elif input_string.find('sec_level') == -1 and input_string.find('root_key_ver') == -1 and input_string.find('type') == -1:
            continue
        for key in args:
            args = fill_arg_dict(input_string, key, args)

    return args


def check_arg(args):
    """
    Check input arguments.
    """
    if args['sec_level'] != '0' and args['sec_level'] != '1' and args['sec_level'] != '2':
        print ("Warning: sec_level invalid, use default 0 instead")
        args['sec_level'] = '0'

    if args['root_key_ver'] != '0' and args['root_key_ver'] != '1' and args['root_key_ver'] != '2' and args['root_key_ver'] != '3':
        print ("Warning: root_key_ver invalid, use default 0 instead")
        args['root_key_ver'] = '0'

    if args['type'] != 'legacy' and args['type'] != 'cert_chain':
        print ("Warning: type invalid, use default 'legacy' instead")
        args['type'] = 'legacy'

    return 0


def dump(args):
    """
    Dump input.
    """
    SLOG_I("in_da_file_path = " + args['in_path'])
    SLOG_I("chip_name = " + args['chip'])
    SLOG_I("bbchip_config_path = " + args['config_path'])
    SLOG_I("load_region_idx = " + args['load_region_idx'])
    SLOG_I("out_da_file_path = " + args['out_path'])
    SLOG_I("sec_level = " + args['sec_level'])
    SLOG_I("root_key_ver = " + args['root_key_ver'])
    SLOG_I("type = " + args['type'])


def resign(args, da_info_entry, chip_config):
    src_file = args['in_path']
    dest_file = args['out_path']
    chip_name = args['chip']
    sec_level = args['sec_level']
    code = dainfo.resign_all(src_file, dest_file, chip_name,
                             da_info_entry, chip_config, sec_level)
    return code


def main():
    load_region_idx = "all"
    if len(sys.argv) < 6:
        print_usage()
        return -1

    args = {'in_path': '', \
            'chip': '', \
            'config_path': '', \
            'load_region_idx': '', \
            'out_path': '', \
            'sec_level': '0', \
            'root_key_ver': '0', \
            'type': 'legacy'}
    args=parse_arg(sys.argv)

    found = 0

    args['in_path'] = sys.argv[1]
    args['chip'] = sys.argv[2]
    args['config_path'] = sys.argv[3]
    args['load_region_idx'] = sys.argv[4]
    args['out_path'] = sys.argv[5]
    check_arg(args)
    dump(args)

    chip_configs = dainfo.BBChips()

    out_path = os.path.dirname(os.path.abspath(args['out_path']))
    if not os.path.exists(out_path):
        os.makedirs(out_path)

    #shutil.copyfile(args['in_path'], args['out_path'])

    chip_configs.load_config(args['config_path'])
    da_info_obj = dainfo.DaInfo()
    da_info_obj.parse(args['in_path'], 0)

    da_resign_done = False
    info_entry_count = 0
    length_change = 0
    # extract da load regions for resign
    for chip_config in chip_configs.chiplist:
        if args['chip'] == chip_config.name:
            for da_info_entry in da_info_obj.m_da_info_entries:
                if dainfo.chip_match(chip_config, da_info_entry) is True:
                    SLOG_I("chip match!: " + args['chip'])
                    found = 1
                    if args['load_region_idx'] == 'all':
                        code = resign(args, da_info_entry, chip_config);
                        if code != CODE_SUCCESS:
                            SLOG_E("RESIGN DA FAILURE")
                            return
                        break
                    else:
                        length_change = dainfo.resign_load_region_with_idx(args['load_region_idx'], args['out_path'], args['chip'], da_info_entry,
                                                           chip_config, args['sec_level'])
                    da_resign_done = True
                    info_entry_count += 1
                    continue
                # update the rest of the region's offset if needed
                if da_resign_done == True:
                    if length_change == 0:
                        break
                    else:
                        dainfo.region_update_after(args['out_path'], da_info_obj, info_entry_count, length_change)
                        if length_change < 0:
                            dainfo.file_truncate(args['out_path'], length_change)
                        break
                info_entry_count += 1
            break

    if found == 0:
        remove_file(args['out_path'])
        SLOG_E("RESIGN DA FAILURE, DA NOT FOUND, CHIP: " + args['chip'])
        SLOG_E("=============================================================")
        SLOG_E("please refer to the above info and fill bbchips.ini correctly")
        SLOG_E("re-sign failed")
        SLOG_E("=============================================================")
        da_info_obj.dump(False)
    else:
        msg = args['chip'].upper() + ' RESIGN DA SUCCSSFULLY, PLEASE CHECK: '
        SLOG_I(msg + os.path.realpath(args['out_path']))





if __name__ == '__main__':
    main()
