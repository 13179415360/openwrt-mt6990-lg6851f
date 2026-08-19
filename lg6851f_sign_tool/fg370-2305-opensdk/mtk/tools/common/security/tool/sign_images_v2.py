"""
This module is for signing images except for preloader and
customer specific format images.
"""

__copyright__ = "Copyright 2017-2021, MediaTek Inc."
__version__ = "3.0.0"

from lib.sign_error import CODE_SUCCESS
from lib.sign_error import CODE_INVALID_INPUT
from lib.sign_error import CODE_FILE_NOT_FOUND
from lib.sign_error import CODE_FILE_TYPE_NOT_MATCHED
from lib.sign_error import CODE_INVALID_CONFIG_FILE
from lib.sign_error import CODE_DATA_INCORRECT
from lib.sign_error import CODE_EXECUTE_FAIL
from lib.sign_error import CODE_INVALD_IMAGE_VER

from lib.sign_util import SLOG_E
from lib.sign_util import SLOG_I
import lib.sign_util as util

import argparse
import struct
import inspect
import json
import os
import re
import hashlib

def merge_images(config_dir, source_dir,
                 intermediate_dir,
                 bundle, align):

    TAG_SIGNLE_BIN = "single_bin"
    TAG_MULTI_BIN = "multi_bin"
    TAG_BUNDLE = "bundle"

    def load_images_config(foldername):
        """Load config file into dictionary

        Args:
            foldername: Full file name of config

        Returns:
            ERROR CODE of execute status
                CODE_SUCCESS: if check with no error
                CODE_INVALID_INPUT: if input value is invalid
                CODE_FILE_NOT_FOUND: if folder is not exist
                CODE_FILE_TYPE_NOT_MATCHED: if folder type is not folder
            Dictionanry object of config

        Raises:
            None
        """
        IMG_VER_FILE_NAME = "img_ver.txt"
        IMG_LIST_FILE_NAME = "img_list.txt"

        img_ver_file = foldername + "/" + IMG_VER_FILE_NAME
        img_list_file = foldername + "/" + IMG_LIST_FILE_NAME

        code, img_ver = util.load_config(img_ver_file)
        if code != CODE_SUCCESS:
            return code, None
        code, img_list = util.load_config(img_list_file)
        if code != CODE_SUCCESS:
            return code, None
        for img in img_list[TAG_SIGNLE_BIN]:
            name = img_list[TAG_SIGNLE_BIN][img]
            if name not in img_ver:
                SLOG_E(name + " is not found in img_ver")
                return CODE_DATA_INCORRECT, None
            img_list[TAG_SIGNLE_BIN][img] = dict()
            img_list[TAG_SIGNLE_BIN][img][name] = img_ver[name]
        for img in img_list[TAG_MULTI_BIN]:
            name_string = img_list[TAG_MULTI_BIN][img]
            names = name_string.split(",")
            if len(names) < 2:
                return CODE_DATA_INCORRECT, None

            img_list[TAG_MULTI_BIN][img] = dict()
            for name in names:
                if name not in img_ver:
                    SLOG_E(name + " is not found in img_ver")
                    return CODE_DATA_INCORRECT, None

                img_list[TAG_MULTI_BIN][img][name] = img_ver[name]
        del(img_ver)
        return CODE_SUCCESS, img_list

    def extract_images(cfg, in_dir, out_dir):
        """Extract image bin from images

        Args:
            cfg: Image configuration

        Returns:
            ERROR CODE of execute status
                CODE_EXECUTE_FAIL: Fail to remove files
                CODE_INVALD_IMAGE_VER: Source images header version is incorrect

        Raises:
            None
        """
        in_dir = os.path.realpath(in_dir)
        out_dir = os.path.realpath(out_dir)

        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)

        # Remove all files in images folder
        for filename in os.listdir(out_dir):
            try:
                if os.path.isfile(filename) or os.path.islink(filename):
                    os.unlink(filename)
                elif os.path.isdir(filename):
                    shutil.rmtree(filename)
            except Exception as e:
                print('Failed to delete %s. Reason: %s' % (filename, e))
                return CODE_EXECUTE_FAIL

        for filename in cfg:
            filenames = filename.split(".")

            file = in_dir + "/" + filenames[0] + "-verified." + filenames[1]
            if not os.path.isfile(file):
                file = in_dir + "/" + filenames[0] + "." + filenames[1]
                if not os.path.isfile(file):
                    continue

            print(file)
            if filenames[0] == "boot":
                binfile = out_dir + "/" + filenames[0]
                # To be implemented for boot image
                print("BOOT image outfile:" + binfile)
                with open(binfile, "w") as outdata:
                     outdata.write("tbd")
                continue;
            with open(file, "rb") as indata:
                while(1):
                    buf = indata.read(80)
                    if buf is None or buf == '' or len(buf) != 80:
                        break
                    data = struct.unpack("<I I 32s 12x I I 8x I 8x", buf)
                    if data[0] == 0x58881688:
                        data_size = data[1]
                        item_file = data[2].strip('\0')
                        hdr_size = data[3]
                        hdr_version = data[4]
                        align = data[5]

                        if hdr_version != 1:
                            return CODE_INVALD_IMAGE_VER

                        totalsize = (data_size + hdr_size + align - 1) / align * align

                        if item_file == "cert1" or item_file == "cert2":
                            indata.seek(totalsize - 80, 1)
                        else:
                            binfile = out_dir + "/" + item_file
                            print("MK image outfile:" + binfile)
                            with open(binfile, "w") as outdata:
                                outdata.write(buf)
                                buf = indata.read(totalsize - 80)
                                outdata.write(buf)
        return CODE_SUCCESS

    def build_hash_image(offset, profile, out_dir, alignment):

        MAGIC = 0x58881688
        SEGM_MAGIC = 0x4D474553
        ITEM_PATTERN = "<I 32s I I I"
        HEADER_PATTEN = "<I I 32s 8s 4I 4x I 8x"
        HEADER_VERSION = 3
        IMG_TYPE_RAW = 0

        # Compute Header size
        hdr_sz = 80 + 16
        data_sz = 0
        for item in profile:
            hdr_sz += struct.calcsize(ITEM_PATTERN)

        los = hdr_sz
        for item in profile:
            item['auth_offset'] = los
            item['auth_size'] = len(item['hash'])
            item['auth_block_size'] = len(item['hash'])
            los += item['size']
            data_sz += len(item['hash'])

        total_sz = hdr_sz + data_sz
        block_sz = (total_sz + (alignment - 1)) / alignment * alignment

        with open(out_dir + "/authentication.block", "w") as outdata:
            buf = struct.pack(HEADER_PATTEN,
                              MAGIC,
                              0, "authentication",
                              b'\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF',
                              MAGIC, block_sz, HEADER_VERSION, IMG_TYPE_RAW, 0)

            outdata.write(buf)
            outdata.write(b'\xFF' * 16)
            for item in profile:
                buf = struct.pack(ITEM_PATTERN,
                                  SEGM_MAGIC, item['name'], item['auth_offset'],
                                  item['auth_size'], item['auth_block_size'])
                outdata.write(buf)

            for item in profile:
                outdata.write(item['hash'])

            outdata.write(b'\xFF' * (block_sz - total_sz))

        return CODE_SUCCESS, block_sz

    def build_bundle(name, profile, hash_name, hash_size, in_dir, out_dir, alignment):

        MAGIC = 0x58881688
        SEGM_MAGIC = 0x4D474553
        ITEM_PATTERN = "<I 32s I I I"
        HEADER_PATTEN = "<I I 32s 8s 4I 4x I 8x"
        HEADER_VERSION = 3
        IMG_TYPE_RAW = 0

        # Compute Header size
        data_sz = 0
        hdr_sz = 80 + 16
        data_sz = 0
        for item in profile:
            hdr_sz += struct.calcsize(ITEM_PATTERN)
            data_sz += item['block_size']

        hdr_sz += struct.calcsize(ITEM_PATTERN)
        data_sz += hash_size

        hdr_block_sz = (hdr_sz + (alignment - 1)) / alignment * alignment
        with open(out_dir + "/" + name, "w") as outdata:
            buf = struct.pack(HEADER_PATTEN, MAGIC,
                      data_sz, name, b'\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF',
                      MAGIC, hdr_block_sz, HEADER_VERSION, IMG_TYPE_RAW, alignment)

            outdata.write(buf)
            outdata.write(b'\xFF' * 16)

            next_offset = 0
            for item in profile:
                buf = struct.pack(ITEM_PATTERN,
                                  SEGM_MAGIC, item['name'], item['offset'] + hdr_block_sz,
                                  item['size'], item['block_size'])
                outdata.write(buf)
                next_offset = item['offset'] + item['block_size']

            buf = struct.pack(ITEM_PATTERN,
                                  SEGM_MAGIC, "authentication", next_offset + hdr_block_sz,
                                  hash_size, hash_size)
            outdata.write(buf)
            outdata.write(b'\xFF' * (hdr_block_sz - hdr_sz))

            for item in profile:
                sz = item['block_size']
                with open(in_dir + "/" + item['name'] + ".block", "r") as indata:
                    print(item['name'])
                    block_len = 0
                    while(sz > 0):
                        if sz > 2048:
                            block_len = 2048
                        else:
                            block_len = sz

                        buf = indata.read(block_len)
                        outdata.write(buf)
                        sz -= block_len

            hash_sz = hash_size
            with open(in_dir + "/" + hash_name + ".block", "r") as indata:

                block_len = 0
                while(hash_sz > 0):
                    if hash_sz > 2048:
                        block_len = 2048
                    else:
                        block_len = hash_sz

                    buf = indata.read(block_len)
                    outdata.write(buf)
                    hash_sz -= block_len

    def build_image(bundle_name, config, in_dir, out_dir, alignment):
        img_list = []
        offset = 0
        for name in config:
            with open(in_dir + "/" + name, "r") as indata:
                buf = indata.read(80)
                if buf is None or buf == '' or len(buf) != 80:
                    break
                data = struct.unpack("<I I 32s 12x I 12x I 8x", buf)
                if data[0] == 0x58881688:
                    data_size = data[1]
                    item_file = data[2].strip('\0')
                    hdr_size = data[3]
                    align = data[4]

                    item = dict()
                    item['name'] = name
                    item['block_size'] = (data[1] + (alignment - 1)) / alignment * alignment
                    item['size'] = data[1]
                    item['hash'] = None
                    item['offset'] = offset
                    offset += item['block_size']

                    # Move cursor to data block
                    indata.seek(hdr_size - 80, 1)

                    with open(in_dir + "/" + name + ".block", "w") as outdata:
                        m = hashlib.sha384()
                        sz = data_size
                        block_len = 0
                        while(sz > 0):
                            if sz > 2048:
                                block_len = 2048
                            else:
                                block_len = sz

                            buf = indata.read(block_len)
                            m.update(buf)
                            outdata.write(buf)
                            sz -= block_len

                        item['hash'] = m.digest()
                        outdata.write('\0' * (item['block_size'] - item['size']))

                    img_list.append(item)

        code, hash_sz = build_hash_image(offset, img_list, in_dir, alignment)
        if code != CODE_SUCCESS:
            return code

        build_bundle(bundle_name, img_list, "authentication",
                     hash_sz, in_dir, out_dir, alignment)
        return CODE_SUCCESS

    code, config = load_images_config(config_dir)
    if code != CODE_SUCCESS:
        return code

    code = extract_images(config[TAG_SIGNLE_BIN], source_dir, intermediate_dir)
    if code != CODE_SUCCESS:
        return code

    code = extract_images(config[TAG_MULTI_BIN], source_dir, intermediate_dir)
    if code != CODE_SUCCESS:
        return code

    for bundle_name in config[TAG_BUNDLE]:
        item = config[TAG_BUNDLE][bundle_name]
        items = item.split(",")
        code = build_image(bundle_name, items, intermediate_dir, source_dir, align)
        if code != CODE_SUCCESS:
            return code
    return CODE_SUCCESS


def execute(arguments):
    """Internal API for execute commands

    Args:
        arguments: Arguments from command line

    Returns:
        None

    Raises:
        None
    """
    code = CODE_SUCCESS
    if arguments.sign:
        pass
    elif arguments.merge:
        code = merge_images(arguments.config_dir, arguments.source_dir,
                            arguments.intermediate_dir,
                            arguments.bundle, arguments.alignment)
        if code == CODE_SUCCESS:
            SLOG_I("Generate Successfully")
        else:
            SLOG_E("Generate Error: " + hex(code))

    else:
        SLOG_E("Unsupported Operation")


if __name__ == '__main__':

    DESCRIPTION = 'Mediatek sign images tools version 3.0'
    SIGN_HELP = "sign images"
    MERGE_HELP = "merge images"
    CONFIG_DIR_HELP = "path of config file"
    SOURCE_DIR_HELP = "path of source file"
    INTERMEDIATE_DIR_HELP = "path of intermediate file"
    BUNDLE_HELP = "specific bundle image"
    ALIGNMENT = "data alignment"

    parser = argparse.ArgumentParser(description = DESCRIPTION)
    parser.add_argument('--sign', help=SIGN_HELP, required=False)
    parser.add_argument('--merge', help=MERGE_HELP, required=False, action='store_true')
    parser.add_argument('--config_dir', help=CONFIG_DIR_HELP, required=True, nargs='?')
    parser.add_argument('--source_dir', help=SOURCE_DIR_HELP, required=True, nargs='?')
    parser.add_argument('--intermediate_dir', help=INTERMEDIATE_DIR_HELP, required=True, nargs='?')
    parser.add_argument('--alignment', help=ALIGNMENT, type=int, required=True, nargs='?')
    parser.add_argument('--bundle', help=BUNDLE_HELP, required=False, nargs='?')

    args = parser.parse_args()
    execute(args)
