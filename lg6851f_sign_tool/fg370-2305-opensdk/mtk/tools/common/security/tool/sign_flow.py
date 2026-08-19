#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
This module integrates image signing flow so user could sign all images
in one shot.
"""

__copyright__ = "Copyright 2022, MediaTek Inc."
__version__ = "3.0.1"

import argparse
import os
import re
import shutil
import stat
import sys
import struct
import traceback
import lib.mkimghdr
import lib.getPublicKey

from lib.asn1_gen import asn1_gen

from lib.sign_error import CODE_SUCCESS
from lib.sign_error import CODE_INVALID_INPUT
from lib.sign_error import CODE_DATA_INCORRECT
from lib.sign_error import CODE_GLOBAL_BLOCK_NOT_FOUND
from lib.sign_error import CODE_WRONG_IMAGE_FORMAT
from lib.sign_error import CODE_UNSUPPORTED_SIGN_FORMAT
from lib.sign_error import CODE_NOT_SUPPORT
from lib.sign_error import CODE_GENERATE_CONFIG_FAIL
from lib.sign_error import CODE_IMAGE_NAME_NOT_MATCHED
from lib.sign_error import CODE_IMAGE_SIZE_NOT_MATCHED
from lib.sign_error import CODE_IMAGE_BIN_NOT_FOUND
from lib.sign_error import CODE_IMAGE_INVALID_IMAGE_TYPE
from lib.sign_error import CODE_INVALID_CONFIG_FILE

from lib.sign_image import image_descriptor
from lib.sign_image import get_vboot10_cert
from lib.sign_image import IMG_TYPE_UNKNOWN
from lib.sign_image import IMG_TYPE_MK_RAW
from lib.sign_image import IMG_TYPE_MK_MD_LTE
from lib.sign_image import IMG_TYPE_MK_MD_C2K
from lib.sign_image import IMG_TYPE_MK_MASK
from lib.sign_image import IMG_TYPE_BOOTING
from lib.sign_image import IMG_TYPE_DTBO
from lib.sign_image import IMG_TYPE_SQUASHFS
from lib.sign_master import SignMaster
from lib.sign_util import SLOG_D
from lib.sign_util import SLOG_E
from lib.sign_util import SLOG_I
from lib.sign_util import SLOG_W
from lib.sign_util import check_file
from lib.sign_util import check_folder
from lib.sign_util import remove_file
from lib.sign_util import load_config
from lib.sign_util import load_image_ini
from lib.sign_util import dump_tree

class SignImage():
    DEBUG = True
    IMG_HDR_MAGIC = 0x58881688
    MKIMG_HDR_FMT = "<2I 32s 2I 8I"

    IMG_TYPE_UNKNOWN = 0
    IMG_TYPE_RAW     = 1
    IMG_TYPE_MD_LTE  = 2
    IMG_TYPE_MD_C2K  = 3

    TAG_SIGNLE_BIN = "single_bin"
    TAG_MULTI_BIN = "multi_bin"
    TAG_BUNDLE = "bundle"
    TAG_IMG_HASH_LIST = "image_hash_list"
    TAG_GLOBAL = "global"

    CERT_REPLACE_TBS_CERTIFICATE = r"(\s)*tbsCertificate"
    CERT_REPLACE_SIG = r"(\s)*sigValue EXTERNAL_BITSTRING"
    CERT_REPLACE_HASH_ALOG_OID = r"(\s)*hashAlgorithm OID"
    CERT_REPLACE_SIG_ALOG_OID = r"(\s)*sigAlgo OID"
    CERT_REPLACE_SALT_LEN = r"(\s)*saltLength INTEGER"
    CERT1_REPLACE_MD_PUBLIC_KEY_HASH = r"(\s)*pubkHash EXTERNAL_BITSTRING"
    CERT1_REPLACE_IMG_ROOT_KEY = r"(\s)*rootpubk EXTERNAL_BITSTRING"
    CERT1_REPLACE_IMG_PUBLIC_KEY1 = r"(\s)*pubk EXTERNAL_DER"
    CERT1_REPLACE_IMG_PUBLIC_KEY2 = r"(\s)*pubk2 EXTERNAL_DER"
    CERT2_REPLACE_HASH = r"(\s)*imgHash EXTERNAL_BITSTRING"
    CERT2_REPLACE_HEADER_HASH = r"(\s)*imgHdrHash EXTERNAL_BITSTRING"
    CERT2_REPLACE_IMG_HASH_MULTI = r"(\s)*imgHash_Multi EXTERNAL_BITSTRING"
    CERT2_REPLACE_SOCID = r"(\s)*socid PRINTABLESTRING"
    CERT2_REPLACE_IMGSIZE = r"(\s)*imgSize INTEGER"
    CERT1MD_REPLACE_TARGET = r"(\s)*pubkHash EXTERNAL_BITSTRING"
    SW_ID_REPLACE_TARGET = r"(\s)*swID INTEGER"
    IMG_VER_REPLACE_TARGET = r"(\s)*imgVer INTEGER"
    SEC_LEVEL_REPLACE_TARGET = r"(\s)*secLevel INTEGER"
    ROOT_KEY_VER_REPLACE_TARGET = r"(\s)*rootKeyVer INTEGER"
    IMG_GROUP_REPLACE_TARGET = r"(\s)*imgGroup INTEGER"
    SEC_LEVEL_REPLACE_TARGET = r"(\s)*secLevel INTEGER"
    ROOT_KEY_VER_REPLACE_TARGET = r"(\s)*rootKeyVer INTEGER"
    MKIMAGE_HDR_FMT = "<2I 32s 2I 8I"
    FAULT_INJECTION_EN = 0x0700
    IS_ENABLE_FAULT_INJECTION = False

    # Constant for Flash-less
    FLB_MAGIC = 0x524C4248
    FLB_HASH_MAGIC = 0x494C4248
    FLB_SEGM_MAGIC = 0x4D474553
    FLB_HEADER_PATTERN = "<I I I H H 32s H 6x I 4x"
    FLB_ITEM_PATTERN = "<I 32s I I I"
    FLB_HASH_HEADER_PATTERN = "<I I I H H H 14x"
    FLB_HASH_ITEM_PATTERN = "<I I 8x 64s"
    FLB_HDR_VERSION = 3

    #  0x03, 0x04, 0x02, 0x01 --> SHA256
    #  0x03, 0x04, 0x02, 0x02 --> SHA384
    ALG_ID_SHA256 = 0x01020403
    ALG_ID_SHA384 = 0x02020403

    def __init__(self):
        self.m_context = {}
        self.m_single_bin = None
        self.m_multi_bin = None
        self.m_image_hash_list = None
        self.m_target = None

    @staticmethod
    def __load_env_config(file, platform, project):

        code, cfg = load_config(file, 'simple')
        if code != CODE_SUCCESS:
            SLOG_E("Load env.cfg failure: " + hex(code))
            return code, None

        base_dir = os.path.dirname(__file__)
        for item in cfg:
            cfg[item] = cfg[item].replace("${PLATFORM}", platform)
            cfg[item] = cfg[item].replace("${PROJECT}", project)
            cfg[item] = os.path.join(base_dir, cfg[item])
            cfg[item] = os.path.realpath(cfg[item])

        if 'PRODUCT_OUT' in os.environ:
            exec_dir = os.getcwd()
            root_dir = os.path.join(exec_dir, os.environ['PRODUCT_OUT']);

            SLOG_I('IN/OUT folder path from environment variables')
            cfg['out_path'] = root_dir
            cfg['in_path'] = root_dir

        cfg['resign_path'] = os.path.join(cfg['out_path'], 'resign')
        cfg['intermediate'] = os.path.join(cfg['out_path'], 'bin', 'multi_tmp')
        cfg['platform'] = platform
        cfg['project'] = project
        cfg['flashless'] = False
        cfg['no_cert'] = False
        cfg['alignment'] = 0

        project_cfg = None
        projects_dir = cfg['projects_dir']
        project_cfg_path = os.path.join(projects_dir, project + ".ini")
        project_default_path = os.path.join(projects_dir, "default.ini")
        code = check_file(project_cfg_path)
        if code == CODE_SUCCESS:
            code, project_cfg = load_config(project_cfg_path, 'simple')
            if code == CODE_SUCCESS:
                if not ('cert1_dir' in project_cfg and \
                   'cert2_key_dir' in project_cfg and \
                   'img_list_path' in project_cfg and \
                   'img_ver_path' in project_cfg and \
                   'sec_level' in project_cfg):
                    project_cfg = None

        if project_cfg is None:
           code = check_file(project_default_path)
           if code == CODE_SUCCESS:
               code, project_cfg = load_config(project_default_path, 'simple')
               if code == CODE_SUCCESS:
                   if not ('cert1_dir' in project_cfg and \
                       'cert2_key_dir' in project_cfg and \
                       'img_list_path' in project_cfg and \
                       'img_ver_path' in project_cfg and \
                       'sec_level' in project_cfg):
                       project_cfg = None

        flashless = False
        no_cert = False
        auth_type = False
        alignment = 16
        if project_cfg is not None:
            sec_level = project_cfg['sec_level']
            if 'flashless' in project_cfg and project_cfg['flashless'] == "1":
                flashless = True
            if 'no_cert' in project_cfg and project_cfg['no_cert'] == "1":
                no_cert = True
            if 'auth_type' in project_cfg and project_cfg['auth_type'] == "1":
                auth_type = True

            if 'alignment' in project_cfg:
                try:
                    alignment = int(project_cfg['alignment'], 10)
                except:
                    if flashless:
                        SLOG_W('Default alignment is 2048')

            del(project_cfg['sec_level'])
            for item in project_cfg:
                if item != 'auth_type' and item !='cert_flag':
                    cfg[item] = project_cfg[item].replace("${SEC_LEVEL}", sec_level)
                    cfg[item] = os.path.join(projects_dir, cfg[item])
                    cfg[item] = os.path.realpath(cfg[item])
                else:
                    cfg[item] = project_cfg[item]

        cfg['flashless'] = flashless
        cfg['no_cert'] = no_cert
        cfg['auth_type'] = auth_type
        cfg['alignment'] = alignment

        return CODE_SUCCESS, cfg

    @staticmethod
    def __load_image_ini(img_list, img_ver, iv, flashless):
        """
        parse image list file and get image signing settings
        """

        root_key_verification = iv['root_key_verification']
        del(iv['root_key_verification'])

        code, img_tree = load_config(img_list)
        if code != CODE_SUCCESS:
            return code, None

        if not flashless:
            if SignImage.TAG_BUNDLE in img_tree:
                del(img_tree[SignImage.TAG_BUNDLE])

        code, img_list = load_config(img_ver)
        if code != CODE_SUCCESS:
            return code, None

        if SignImage.TAG_GLOBAL in img_tree:
            settings = img_tree[SignImage.TAG_GLOBAL]
        else:
            settings = dict()

        board_avb_enable = False
        if 'board_avb_enable' in iv:
            if iv['board_avb_enable'] == 'true':
                board_avb_enable = True
            del(iv['board_avb_enable'])

        # Override Global by IV
        if iv is not None:
            for item in iv:
                settings[item] = iv[item]

        if 'root_key' not in settings or 'root_key' not in settings:
            return CODE_GLOBAL_BLOCK_NOT_FOUND, None

        is_hsm = 0
        padding = 'pss'
        socid = ''
        base_dir = os.path.dirname(__file__)
        key = settings['key']
        root_key = None

        if 'hsm' in settings:
            if settings['hsm'] == '1':
                is_hsm = 1

        root_key = settings['root_key']
        if not is_hsm:
            root_key = os.path.join(base_dir, root_key)
            root_key = os.path.realpath(root_key)

        if 'sig_pad' in settings:
            padding = settings['sig_pad']
        if 'socid' in settings:
            socid = settings['socid']

        # Check and Organize the Properties of Image in List
        for image_name in img_list:
            image = img_list[image_name]
            image['name'] = image_name
            image['root_key'] = root_key
            image['socid'] = socid
            if 'hsm' in image:
                if image['hsm'] == '1':
                    image['hsm'] = 1
                else:
                    image['hsm'] = 0
            else:
                image['hsm'] = is_hsm
            if 'key' not in image:
                image['key'] = key
            if image['hsm'] == 0:
                image['key'] = os.path.join(base_dir, image['key'])
                image['key'] = os.path.realpath(image['key'])
                code = check_file(image['key'])
                if code != CODE_SUCCESS:
                    SLOG_E("Global Key or Local Key are both invalid")
                    return code, None

            if 'img_ver' not in image:
                SLOG_E('img_ver is not found in ' + image_name)
                return CODE_DATA_INCORRECT, None
            else:
                image['img_ver'] = int(image['img_ver'])

            if 'img_group' not in image:
                SLOG_E('img_group is not found in ' + image_name)
                return CODE_DATA_INCORRECT, None
            else:
                image['img_group'] = int(image['img_group'])

            if 'img_sec_level' not in image:
                image['img_sec_level'] = 0
            else:
                image['img_sec_level'] = int(image['img_sec_level'])

            if SignImage.IS_ENABLE_FAULT_INJECTION:
                image['img_sec_level'] = image['img_sec_level'] & \
                                         SignMaster.FAULT_INJECTION_EN

            if 'root_key_ver' not in image:
                image['root_key_ver'] = 0
            else:
                image['root_key_ver'] = int(image['root_key_ver'])

            if 'sw_id' not in image:
                image['sw_id'] = 0
            else:
                image['sw_id'] = int(image['sw_id'])

            if 'sig_pad' not in image:
                image['sig_pad'] = padding
            else:
                image['sig_pad'] = 'pss'

        single = None
        multi = None
        hash_list = None
        bundle = None
        if flashless:
            if SignImage.TAG_BUNDLE not in img_tree:
                return CODE_DATA_INCORRECT, None
            bundle = img_tree[SignImage.TAG_BUNDLE]
            for item in bundle:
                split_item = [s.strip() for s in bundle[item].split(',')]
                bundle[item] = []
                for s_item in split_item:
                    if s_item in img_list:
                        img_list[s_item]['bundle'] = True
                        bundle[item].append(img_list[s_item])

        if SignImage.TAG_SIGNLE_BIN not in img_tree:
            return CODE_DATA_INCORRECT, None

        # Re-Organize the Image Tree with Image List
        single = img_tree[SignImage.TAG_SIGNLE_BIN]
        for item in single:
            if board_avb_enable and \
               ('boot.img' == item or 'recovery' == item):
                continue
            if single[item] in img_list:
                single[item] = [img_list[single[item]]]

        multi = img_tree[SignImage.TAG_MULTI_BIN]
        for item in multi:
            split_item = [s.strip() for s in multi[item].split(',')]
            multi[item] = []
            for s_item in split_item:
                if s_item in img_list:
                    multi[item].append(img_list[s_item])

        hash_list = img_tree[SignImage.TAG_IMG_HASH_LIST]
        for item in hash_list:
            if '.' in item and "squashfs" == item.split('.')[1]:
                new_item = item.split('.')[0].strip() + "_ro"
                hash_list[item] = [img_list[new_item]]
            else:
                del(hash_list[item])

        single = None
        multi = None
        hash_list = None
        bundle = None
        del(img_tree[SignImage.TAG_GLOBAL])
        del(img_list)

        return CODE_SUCCESS, img_tree

    @staticmethod
    def create(cfg_file, platform, project, iv = None):
        code, env_cfg = SignImage.__load_env_config(cfg_file, platform, project)
        if code != CODE_SUCCESS:
            return code, None

        code = check_folder(env_cfg['in_path'])
        if code != CODE_SUCCESS:
            SLOG_E("IN_DIR is invalid")
            return code, None
        code = check_folder(env_cfg['out_path'])
        if code != CODE_SUCCESS:
            SLOG_E("OUT_DIR is invalid")
            return code, None
        code = check_folder(env_cfg['resign_path'])
        if code != CODE_SUCCESS:
            SLOG_E("RESIGN_DIR is invalid")
            return code, None
        code = check_folder(env_cfg['intermediate'])
        if code != CODE_SUCCESS:
            SLOG_E("INTERMEDIATE_DIR is invalid")
            return code, None
        code = check_folder(env_cfg['cert1_dir'])
        if code != CODE_SUCCESS:
            SLOG_E("CERT1_DIR is invalid")
            return code, None
        code = check_folder(env_cfg['cert2_key_dir'])
        if code != CODE_SUCCESS:
            SLOG_E("CERT2_KEY DIR is invalid")
            return code, None
        code = check_folder(env_cfg['x509_template_path'])
        if code != CODE_SUCCESS:
            SLOG_E("X509_TEMPLATE_DIR is invalid")
            return code, None

        target = None
        if iv is not None and 'target' in iv:
            target = iv['target'].strip()
            del(iv['target'])

        img_list = env_cfg['img_list_path']
        img_ver = env_cfg['img_ver_path']
        del(env_cfg['img_list_path'])
        del(env_cfg['img_ver_path'])

        flls = env_cfg['flashless']
        code, img_tree = SignImage.__load_image_ini(img_list, img_ver, iv, flls)
        if code != CODE_SUCCESS:
            SLOG_E("Load Image INI is invalid")
            return code, None

        # Extract image files for flashless cases
        if flls:
            in_dir = env_cfg['in_path']
            out_dir = env_cfg['out_path']
            flashless_dir = os.path.join(out_dir, 'flashless')

            img_list = img_tree[SignImage.TAG_SIGNLE_BIN]
            code = SignImage.__extract_images(img_list, in_dir, flashless_dir)
            if code != CODE_SUCCESS:
                return code, None

            img_list = img_tree[SignImage.TAG_MULTI_BIN]
            code = SignImage.__extract_images(img_list, in_dir, flashless_dir)
            if code != CODE_SUCCESS:
                return code, None

        si = SignImage()
        si.m_target = target
        si.m_common = env_cfg
        si.m_flashless = flls

        if target is not None:
            newset = None
            if si.m_flashless:
                for key in img_tree[SignImage.TAG_BUNDLE]:
                    if key == target:
                        newset = dict()
                        newset[key] = img_tree[SignImage.TAG_BUNDLE][key]
                        break
                if newset is not None:
                    img_tree[SignImage.TAG_BUNDLE] = newset

            for key in img_tree[SignImage.TAG_SIGNLE_BIN]:
                if key == target:
                    newset = dict()
                    newset[key] = img_tree[SignImage.TAG_SIGNLE_BIN][key]
                    break
            if newset is not None:
                img_tree[SignImage.TAG_SIGNLE_BIN] = newset

            newset = None
            for key in img_tree[SignImage.TAG_MULTI_BIN]:
                if key == target:
                    newset = dict()
                    newset[key] = img_tree[SignImage.TAG_MULTI_BIN][key]
                    break
            if newset is not None:
                img_tree[SignImage.TAG_MULTI_BIN] = newset

            newset = None
            for key in img_tree[SignImage.TAG_IMG_HASH_LIST]:
                if key == target:
                    newset = dict()
                    newset[key] = img_tree[SignImage.TAG_IMG_HASH_LIST][key]
                    break
            if newset is not None:
                img_tree[SignImage.TAG_IMG_HASH_LIST] = newset

        if si.m_flashless:
            if SignImage.TAG_BUNDLE in img_tree:
                si.m_bundle = img_tree[SignImage.TAG_BUNDLE]
            else:
                si.m_bundle = dict()
        else:
            if SignImage.TAG_SIGNLE_BIN in img_tree:
                si.m_single_bin = img_tree[SignImage.TAG_SIGNLE_BIN]
            else:
                si.m_single_bin = dict()

            if SignImage.TAG_MULTI_BIN in img_tree:
                si.m_multi_bin = img_tree[SignImage.TAG_MULTI_BIN]
            else:
                si.m_multi_bin = dict()

            if SignImage.TAG_IMG_HASH_LIST in img_tree:
                si.m_image_hash_list = img_tree[SignImage.TAG_IMG_HASH_LIST]
            else:
                si.m_image_hash_list = dict()

        del(img_tree)
        img_tree = None
        return CODE_SUCCESS, si

    def __check_image_list(self, bundle_name):
        """
        Check if image file exists
        """
        bundle = self.m_bundle
        out_dir = self.m_common['out_path']
        del_image = dict()
        segment_dir = os.path.join(out_dir, 'flashless')

        for img_bin in bundle[bundle_name]:
            segment_src_file = os.path.join(segment_dir, img_bin['name'])
            code = check_file(segment_src_file)
            if code != CODE_SUCCESS:
                SLOG_W('Image: ' + img_bin['name'] + ' does not exist.')
                del_image[img_bin['name']] = img_bin
        
        for img in del_image :
            bundle[bundle_name].remove(del_image[img])

        return code

    def __fill_cert_config(self, cert_path, replace_set):
        """
        We provide certificate configuration template and could
        generate cerificate based on the configuration. Before that,
        you need to fill up the configuration template so certificate
        generation engine knows how to generate certificate.
        """

        cert_file = os.path.basename(cert_path)
        cert_dir = os.path.dirname(cert_path)
        new_file = os.path.join(cert_dir, cert_file + '.new')

        try:
            remove_file(new_file)
            with open(cert_path, 'r') as template:
                with open(new_file, 'w') as new_config:
                    while True:
                        line = template.readline()
                        if line == None or line == '':
                            break
                        is_found = False
                        index = 0
                        for replace_item in replace_set:
                            index = index + 1
                            rep = re.compile(replace_item['pattern'])
                            if rep.match(line):
                                header = line.split('::=')[0]
                                line2 = header + '::= '
                                line2 = line2 + replace_item['value'] + '\n'
                                new_config.write(line2)
                                is_found = True
                                del(replace_set[index - 1])
                                break
                        if not is_found:
                            new_config.write(line)
                            index = index + 1
                            rep = re.compile(replace_item['pattern'])

            remove_file(cert_path)
            os.rename(new_file, cert_path)
        except:
            return CODE_GENERATE_CONFIG_FAIL
        return CODE_SUCCESS

    def __gen_config(self, image_name, img_type, bin_name,
                     cert_type, is_bundle = False):
        """
        Generate the path for certificae generation
        """
        in_path = self.m_common['in_path']
        out_path = self.m_common['out_path']
        if is_bundle:
            flashless_dir = os.path.join(out_path, 'flashless')
            img_file = os.path.join(flashless_dir, bin_name)
        else:
            img_file = os.path.join(in_path, image_name)

        pkg = os.path.splitext(image_name)[0]
        cert1_path = self.m_common['cert1_dir']
        resign_path = self.m_common['resign_path']
        x509_template_path = self.m_common['x509_template_path']
        mkimage_tool_path = self.m_common['mkimage_tool_path']

        cert_path = os.path.join(resign_path, "cert", pkg, bin_name)
        tmpcert_name = os.path.join(cert_path, "tmp.der")

        code = check_folder(cert_path)
        if code != CODE_SUCCESS:
            return code, None

        if cert_type == 'cert1' and (img_type == 2 or img_type == 3):
           cert_type = 'cert1md'

        cert_der = ''
        dm_cert_der = ''
        hash_path = ''
        config = ''
        config_out = ''
        bin_path = ''
        sig_path = ''
        cert_file = ''

        # Setup Common Path
        work_path = os.path.join(cert_path, cert_type)

        # Setup Stage1 Path
        intermediate = os.path.join(work_path, 'intermediate')
        code = check_folder(intermediate)
        if code != CODE_SUCCESS:
            return code, None

        pubk_path = os.path.join(intermediate, cert_type + "_pubk.der")
        root_pubk_path = os.path.join(intermediate, cert_type + "_rpubk.der")
        root_pubk_proprietary = os.path.join(intermediate, cert_type + "_rpubk")
        tbs_cert_path = os.path.join(intermediate, "tbs_" + cert_type + ".der")
        x509_config = os.path.join(x509_template_path, "x509cert_template.cfg")
        x509_cert = os.path.join(intermediate, "x509_cert.der")

        cert1_file = None
        cert2_file = None
        hash_table_file = None
        if is_bundle:
            cert1_file = os.path.join(cert1_path, pkg + "_cert1.der")
            cert2_file = os.path.join(intermediate, pkg + "_cert2.der")
            hash_table_file = os.path.join(intermediate, pkg + "_hashtable.bin")
            bundle_header_file = os.path.join(intermediate, pkg + "_hdr.bin")
        else:
            cert1_file = os.path.join(cert1_path, bin_name + "_cert1.der")
            cert2_file = os.path.join(intermediate, bin_name + "_cert2.der")
            hash_table_file = None
            bundle_header_file = None

        if cert_type == "cert1md":
            hash_path = os.path.join(intermediate, "hash")
            code = check_folder(hash_path)
            if code != CODE_SUCCESS:
                return code, None
            config = os.path.join(x509_template_path, "cert1md.cfg")
            config_out = os.path.join(intermediate, "cert1md.cfg")

        elif cert_type == "cert1":
            config = os.path.join(x509_template_path, "cert1.cfg")
            config_out = os.path.join(intermediate, "cert1.cfg")

        elif cert_type == "cert2":
            hash_path = os.path.join(intermediate, "hash")
            code = check_folder(hash_path)
            if code != CODE_SUCCESS:
                return code, None

            config = os.path.join(x509_template_path, "cert2.cfg")
            config_out = os.path.join(intermediate, "cert2.cfg")
            dm_cert_der = os.path.join(intermediate, "dm_cert.der")
            bin_path = os.path.join(intermediate, "tmp_bin")
            code = check_folder(bin_path)
            if code != CODE_SUCCESS:
                return code, None
            sig_path = os.path.join(out_path, "sig", pkg)
            code = check_folder(sig_path)
            if code != CODE_SUCCESS:
                return code, None
        else:
            return CODE_UNSUPPORTED_SIGN_FORMAT, None

        code = check_file(config)
        if code != CODE_SUCCESS:
            return code, None

        stage1 = dict()
        stage1['image_file'] = img_file
        stage1['dm_cert_der'] = dm_cert_der
        stage1['hash_path'] = hash_path
        stage1['config'] = config
        stage1['config_out'] = config_out
        stage1['bin_path'] = bin_path
        stage1['intermediate'] = intermediate
        stage1['pubk_path'] = pubk_path
        stage1['root_pubk_path'] = root_pubk_path
        stage1['root_pubk_proprietary'] = root_pubk_proprietary
        stage1['tbs_cert_path'] = tbs_cert_path

        del(dm_cert_der)
        del(hash_path)
        del(config)
        del(config_out)
        del(intermediate)
        del(pubk_path)
        del(root_pubk_path)
        del(root_pubk_proprietary)

        intermediate = os.path.join(work_path, 'cert_intermediate')
        code = check_folder(intermediate)
        if code != CODE_SUCCESS:
            return code, None

        # Setup Stage 2 Path
        hash_path = os.path.join(intermediate, "tbs_" + cert_type + ".hash")
        sig_path = os.path.join(intermediate, "tbs_" + cert_type + ".sig")
        x509_filename = "x509_" + cert_type + ".cfg"
        x509_config_out = os.path.join(intermediate, x509_filename)
        del(x509_filename)

        stage2 = dict()
        stage2['x509_cert'] = x509_cert
        stage2['hash_path'] = hash_path
        stage2['sig_path'] = sig_path
        stage2['tbs_cert_path'] = tbs_cert_path
        stage2['x509_config'] = x509_config
        stage2['x509_config_out'] = x509_config_out
        stage2['mkimage_tool_path'] = mkimage_tool_path

        del(x509_cert)
        del(hash_path)
        del(sig_path)
        del(tbs_cert_path)

        bin_file = os.path.join(bin_path, bin_name + ".bin")
        remove_file(bin_file)

        output = dict()
        output['cert1'] = cert1_file
        output['cert2'] = cert2_file
        output['hashtable'] = hash_table_file
        output['bundle_header'] = bundle_header_file
        output['bin'] = bin_file

        cfg = dict()
        cfg['stage1'] = stage1
        cfg['stage2'] = stage2
        cfg['output'] = output

        return CODE_SUCCESS, cfg

    def __padding_file(self, input_file, align_num):
        """
        Fill 0 to make input_file size multiple of align_num.
        """
        filesize = os.stat(input_file).st_size
        with open(input_file, 'ab+') as file1:
            padding = filesize % align_num
            if padding != 0:
                padding = align_num - padding
                file1.write(b"\x00" * padding)

    def __img_split(self, img, data_path, hdr_size):
        """
        split image into header and image body
        """
        split_header = os.path.join(data_path, "header.bin")
        split_image = os.path.join(data_path, "image.bin")

        remove_file(split_header)
        remove_file(split_image)

        with open(img, 'rb') as src:
            with open(split_header, 'wb') as header:
                header.write(src.read(hdr_size))
            with open(split_image, 'wb') as image:
                image.write(src.read())

        self.__padding_file(split_image, 16)
        return split_header, split_image

    def gen_cert(self, cfg, properties, img_type, cert_type,
                 out_dir, is_last, is_mkimage = True):
        """Generate X509 certificate after generating TBS_CERTIFICATE

        Args:
            image_name: image name of image
            cfg: environment setting to indentify the location of files
            properties: properties of the binary in image
            img_type: image Type
            cert_type: identify cert1 or cert2
            is_last: is the last entity

        Returns:
            code: The return error message. 0 for success

        """
        config = cfg['stage2']
        x509_cert = config['x509_cert']
        tbs_cert_path = config['tbs_cert_path']
        sig_path = config['sig_path']
        x509_config = config['x509_config']
        x509_config_out = config['x509_config_out']
        mkimage_tool_path = config['mkimage_tool_path']
        img_sec_level = properties['img_sec_level']
        bin_name = properties['name']
        sig_pad = properties['sig_pad']
        is_hsm = properties['hsm']
        if cert_type == 'cert1':
            key = properties['root_key']
        elif cert_type == 'cert2':
            key = properties['key']
        else:
            return CODE_NOT_SUPPORT

        sec_level = img_sec_level & 0xFF

        replace_set = []
        replace_item = {}
        replace_item['pattern'] = SignImage.CERT_REPLACE_HASH_ALOG_OID
        flag = ''
        salt = 32
        if sec_level == 0:
            flag = 'RSA2048/SHA256'
            replace_item['value'] = '2.16.840.1.101.3.4.2.1'
            salt = 32
        elif sec_level == 1:
            flag = 'RSA3072/SHA384'
            replace_item['value'] = '2.16.840.1.101.3.4.2.2'
            salt = 48
        elif sec_level == 2:
            flag = 'RSA4096/SHA384'
            replace_item['value'] = '2.16.840.1.101.3.4.2.2'
            salt = 48
        else:
            return CODE_NOT_SUPPORT

        replace_set.append(replace_item)
        replace_item = None

        if sig_pad == 'pss':
            flag = flag + "/PSS"

        code, master = SignMaster.create()
        if code != CODE_SUCCESS:
            return code
        code, signature = master.sign(bool(is_hsm), key, flag, tbs_cert_path)
        if code != CODE_SUCCESS:
            return code

        remove_file(sig_path)
        with open(sig_path, 'wb') as sig_file:
            sig_file.write(signature)

        remove_file(x509_config_out)
        shutil.copy2(x509_config, x509_config_out)
        os.chmod(x509_config, stat.S_IWRITE + stat.S_IREAD)

        replace_item = {}
        replace_item['value'] = tbs_cert_path
        replace_item['pattern'] = SignImage.CERT_REPLACE_TBS_CERTIFICATE
        replace_set.append(replace_item)
        replace_item = None
        replace_item = {}
        replace_item['value'] = sig_path
        replace_item['pattern'] = SignImage.CERT_REPLACE_SIG
        replace_set.append(replace_item)
        replace_item = None
        replace_item = {}
        replace_item['value'] = str(salt)
        replace_item['pattern'] = SignImage.CERT_REPLACE_SALT_LEN
        replace_set.append(replace_item)

        code = self.__fill_cert_config(x509_config_out, replace_set)
        if code != CODE_SUCCESS:
            return code

        remove_file(x509_cert)
        asn1_gen(x509_config_out, x509_cert, False)
        SLOG_D("[OK] X509: " + x509_cert)

        if is_mkimage:
            img_hdr = lib.mkimghdr.mkimage_hdr()
            mkimage_cfg = {}

            if cert_type == 'cert1':
                mkimage_cfg['IMG_LIST_END'] = 0
                if img_type == SignImage.IMG_TYPE_MD_C2K or \
                   img_type == SignImage.IMG_TYPE_MD_LTE:
                    mkimage_cfg['IMG_TYPE'] = 0x2 << 24 | 0x1
                    mkimage_cfg['NAME'] = 'cert1md'
                else:
                    mkimage_cfg['IMG_TYPE'] = 0x2 << 24
                    mkimage_cfg['NAME'] = 'cert1'
            else:
                if is_last:
                    mkimage_cfg['IMG_LIST_END'] = 1
                else:
                    mkimage_cfg['IMG_LIST_END'] = 0
                mkimage_cfg['IMG_TYPE'] = 0x2 << 24 | 0x2
                mkimage_cfg['NAME'] = 'cert2'

            output = cfg['output']
            save_cert_path = output[cert_type]
            remove_file(save_cert_path)

            img_hdr.update_mkimage_hdr(x509_cert, mkimage_tool_path)
            img_hdr.update_mkimage_hdr_by_config(mkimage_cfg)
            img_hdr.pack()
            img_hdr.output(x509_cert, save_cert_path)
        else:
            img_hdr = lib.mkimghdr.mkimage_hdr()
            mkimage_cfg = {}
            if cert_type == 'cert1':
                mkimage_cfg['IMG_LIST_END'] = 0
                #mkimage_cfg['IMG_TYPE'] = 0x2 << 24
                mkimage_cfg['NAME'] = 'cert1'
            else:
                mkimage_cfg['IMG_LIST_END'] = 1
                #mkimage_cfg['IMG_TYPE'] = 0x2 << 24 | 0x2
                mkimage_cfg['NAME'] = 'cert2'

            output = cfg['output']
            save_cert_path = output[cert_type]
            code = remove_file(save_cert_path)
            if code != CODE_SUCCESS:
                return code
            img_hdr.update_mkimage_hdr(x509_cert, mkimage_tool_path)
            img_hdr.update_mkimage_hdr_by_config(mkimage_cfg)
            img_hdr.pack()
            img_hdr.output(x509_cert, save_cert_path)


        SLOG_I("[OK] MK-X509: " + save_cert_path)
        return CODE_SUCCESS

    def gen_tbs_cert1(self, image_name, cfg, properties, img_type):
        """The 1st stage of generate cert1 is to
           generate the TBS_CERTIFICATE part of cert1

        Args:
            image_name: image name of image
            cfg: environment setting to indentify the location of files
            properties: properties of the binary in image

        Returns:
            code: The return error message. 0 for success

        """
        config = cfg['stage1']
        cert_config = config['config_out']
        code = remove_file(cert_config)
        if code != CODE_SUCCESS:
            return code

        shutil.copy2(config['config'], cert_config)
        sw_id = properties['sw_id']
        img_ver = properties['img_ver']
        img_group = properties['img_group']
        img_sec_level = properties['img_sec_level']
        root_key_ver = properties['root_key_ver']
        hdr_size = properties['header_size']

        sig_pad = 'pss'
        if properties['sig_pad'] == '':
            sig_pad = 'legacy'
        else:
            sig_pad = properties['sig_pad']

        replace_set = []
        if int(sw_id) != 0:
            replace_item = {}
            replace_item['value'] = str(sw_id)
            replace_item['pattern'] = SignImage.SW_ID_REPLACE_TARGET
            replace_set.append(replace_item)
        if int(img_ver) != 0:
            replace_item = {}
            replace_item['value'] = str(img_ver)
            replace_item['pattern'] = SignImage.IMG_VER_REPLACE_TARGET
            replace_set.append(replace_item)
        if int(img_group) != 0:
            replace_item = {}
            replace_item['value'] = str(img_group)
            replace_item['pattern'] = SignImage.IMG_GROUP_REPLACE_TARGET
            replace_set.append(replace_item)
        if int(img_sec_level) > 0:
            replace_item = {}
            replace_item['value'] = str(img_sec_level)
            replace_item['pattern'] = SignImage.SEC_LEVEL_REPLACE_TARGET
            replace_set.append(replace_item)
        if int(root_key_ver) > 0:
            replace_item = {}
            replace_item['value'] = str(root_key_ver)
            replace_item['pattern'] = SignImage.ROOT_KEY_VER_REPLACE_TARGET
            replace_set.append(replace_item)

        key = properties['key']
        is_hsm = properties['hsm']
        root_key = properties['root_key']
        sec_level = img_sec_level & 0xFF

        replace_item = {}
        replace_item['pattern'] = SignImage.CERT_REPLACE_SIG_ALOG_OID

        flag = ''
        flag2 = ''
        if sec_level == 0:
            flag = 'RSA2048'
            flag2 = 'SHA256'
            replace_item['value'] = '1.2.840.113549.1.1.11'
        elif sec_level == 1:
            flag = 'RSA3072'
            flag2 = 'SHA384'
            replace_item['value'] = '1.2.840.113549.1.1.12'
        elif sec_level == 2:
            flag = 'RSA4096'
            flag2 = 'SHA384'
            replace_item['value'] = '1.2.840.113549.1.1.12'
        else:
            return CODE_NOT_SUPPORT

        replace_set.append(replace_item)
        code, master = SignMaster.create()
        if code != CODE_SUCCESS:
            return code

        if img_type == SignImage.IMG_TYPE_MD_LTE or \
           img_type == SignImage.IMG_TYPE_MD_C2K:

            hash_path = config['hash_path']
            md_key_path = os.path.join(hash_path, "modem_key.bin")
            md_key_hash_path = os.path.join(hash_path, "modem_key_hash.bin")
            modem = config['image_file']
            hdr_split, md_raw = self.__img_split(modem, hash_path, hdr_size)

            hash_value = b''
            remove_file(md_key_path)
            remove_file(md_key_hash_path)
            if img_type == SignImage.IMG_TYPE_MD_LTE:
                md1_handler = lib.getPublicKey.md1_image()
                found = md1_handler.parse(md_raw)
                if found:
                    code, hash_value = md1_handler.toHash(md_raw, md_key_path)
                    if code != CODE_SUCCESS:
                        return code
                    SLOG_I("MD1 PUBLIC KEY EXPORTED")
            elif img_type == SignImage.IMG_TYPE_MD_C2K:
                md3_handler = lib.getPublicKey.md3_image()
                found = md3_handler.parse(md_raw)
                if found:
                    md3_handler.output(md_raw, md_key_path)
                    code, hash_value = md1_handler.toHash(md_raw, md_key_path)
                    if code != CODE_SUCCESS:
                        return code
                    SLOG_I("MD3 PUBLIC KEY EXPORTED")
            else:
                return CODE_IMAGE_INVALID_IMAGE_TYPE

            remove_file(md_key_hash_path)
            with open(md_key_hash_path, 'wb') as md_hash_out:
                md_hash_out.write(hash_value)

            replace_item = {}
            replace_item['value'] = md_key_hash_path
            replace_item['pattern'] = SignImage.CERT1_REPLACE_MD_PUBLIC_KEY_HASH
            replace_set.append(replace_item)
            del(replace_item)

        code, pubk = master.public_key(bool(is_hsm), key, flag)
        if code != CODE_SUCCESS:
            return code

        intermediate = config['intermediate']
        pubk_path = config['pubk_path']
        root_pubk_path = config['root_pubk_path']
        root_pubk_pro = config['root_pubk_proprietary']

        remove_file(pubk_path)
        remove_file(root_pubk_path)

        code = master.pubk_to_pkcs8(flag, pubk, pubk_path, None)
        if code != CODE_SUCCESS:
            return code

        del(pubk)
        code, root_pubk = master.public_key(bool(is_hsm), root_key, flag)
        if code != CODE_SUCCESS:
            return code

        code = master.pubk_to_pkcs8(flag, root_pubk, root_pubk_path, None)
        if code != CODE_SUCCESS:
            return code

        code = master.pubk_to_proprietary(flag, root_pubk, root_pubk_pro)
        if code != CODE_SUCCESS:
            return code

        replace_item = {}
        replace_item['value'] = pubk_path
        replace_item['pattern'] = SignImage.CERT1_REPLACE_IMG_PUBLIC_KEY2
        replace_set.append(replace_item)
        del(replace_item)

        replace_item = {}
        replace_item['value'] = root_pubk_path
        replace_item['pattern'] = SignImage.CERT1_REPLACE_IMG_PUBLIC_KEY1
        replace_set.append(replace_item)
        del(replace_item)

        replace_item = {}
        replace_item['value'] = root_pubk_pro
        replace_item['pattern'] = SignImage.CERT1_REPLACE_IMG_ROOT_KEY
        replace_set.append(replace_item)
        del(replace_item)

        code = self.__fill_cert_config(cert_config, replace_set)
        if code != CODE_SUCCESS:
            return code

        tbs_cert_path = config['tbs_cert_path']
        remove_file(tbs_cert_path)
        asn1_gen(cert_config, tbs_cert_path, False)

        SLOG_D("GENERATE_TBS_CERTIFICATE_DONE")
        return CODE_SUCCESS

    def gen_all_cert1(self):
        """
        Generate all key certificates
        """
        if self.m_target is not None:
            # Clean up folder of cert1
            cert1_dir = self.m_common['cert1_dir']
            for old_file in os.listdir(cert1_dir):
                os.remove(os.path.join(cert1_dir, old_file))

        all_images = dict()
        if self.m_flashless:
            all_images.update(self.m_bundle)
            for key in all_images:
                SLOG_I("Sign cert1 of " + key)
                code = self.__check_image_list(key)
                if code != CODE_SUCCESS:
                    pass

                if len(all_images[key]) == 0:
                    SLOG_W('Length of ' + key +' images shall not be zero')
                    continue

                if 'header_size' not in all_images[key][0]:
                    SLOG_W('header_size of ' + key +' shall not be zero')
                    continue

                img_type = 0
                bin_obj = all_images[key][0]
                if 'MD' in bin_obj and bin_obj['MD'] is not None:
                    if 'MD_LTE' == bin_obj['MD']:
                        img_type = SignImage.IMG_TYPE_MD_LTE
                    elif 'MD_C2K' == bin_obj['MD']:
                        img_type = SignImage.IMG_TYPE_MD_C2K
                    else:
                        SLOG_E("Unknown MD type")
                        return CODE_DATA_INCORRECT

                bin_name = bin_obj['name']
                code, cfg = self.__gen_config(key, img_type, bin_name,
                                              'cert1', True)
                if code != CODE_SUCCESS:
                    SLOG_E('GENERATE CERT1 FAILURE: ' + hex(code))
                    return code

                code = self.gen_tbs_cert1("flashless", cfg, bin_obj, img_type)
                if code != CODE_SUCCESS:
                    SLOG_E('GENERATE CERT1_TBS FAILURE: ' + hex(code))
                    return code

                out_path = cfg['stage1']['bin_path']
                code = self.gen_cert(cfg, bin_obj, img_type, 'cert1', out_path,
                                     False, False)
                if code != CODE_SUCCESS:
                    SLOG_E('GENERATE CERT1_CERTIFICATE FAILURE: ' + hex(code))
                    return code

            return CODE_SUCCESS
        else:
            all_images.update(self.m_single_bin)
            all_images.update(self.m_multi_bin)
            all_images.update(self.m_image_hash_list)

        in_path = self.m_common['in_path']
        for key in all_images:
            image_file = os.path.join(in_path, key)
            code, descriptor = image_descriptor(image_file)
            if code != CODE_SUCCESS:
                SLOG_W("IMAGE NOT FOUND: " + key)

            image = all_images[key]
            img_type = 0
            hdr_size = 0
            if descriptor is None:
                img_type = IMG_TYPE_MK_RAW
                hdr_size = 0
            else:
                img_type = descriptor['type']
                hdr_size = descriptor['header_size']

            for bin_obj in image:
                bin_name = bin_obj['name']
                SLOG_I("Sign " + bin_name)

                code, cfg = self.__gen_config(key, img_type, bin_name,
                                              'cert1', False)
                if code != CODE_SUCCESS:
                    SLOG_E('GENERATE CERT1 FAILURE: ' + hex(code))
                    return code

                bin_obj['header_size'] = hdr_size
                code = self.gen_tbs_cert1(key, cfg, bin_obj, img_type)
                if code != CODE_SUCCESS:
                    SLOG_E('GENERATE CERT1_TBS FAILURE: ' + hex(code))
                    return code
                out_path = cfg['stage1']['bin_path']
                code = self.gen_cert(cfg, bin_obj, img_type, 'cert1', out_path,
                                     False)
                if code != CODE_SUCCESS:
                    SLOG_E('GENERATE CERT1_CERTIFICATE FAILURE: ' + hex(code))
                    return code

        return CODE_SUCCESS

    def __scan_mkimage(self, image_file, bin_list):
        filesize = os.path.getsize(image_file)
        fixedsize = struct.calcsize(SignImage.MKIMAGE_HDR_FMT)
        offset = 0
        with open(image_file, 'rb') as src:
            while filesize > 0:
                if filesize < fixedsize:
                    SLOG_E("CODE_IMAGE_SIZE_NOT_MATCHED")
                    return CODE_IMAGE_SIZE_NOT_MATCHED

                raw = src.read(fixedsize)
                decoded = struct.unpack(SignImage.MKIMAGE_HDR_FMT, raw)

                dsize = decoded[1]
                hdr_size = decoded[6]
                align_size = decoded[10]
                img_type = decoded[8]
                bin_name = ''

                img_size = dsize + hdr_size + (align_size - 1)
                img_size = (img_size // align_size) * align_size

                img_type_byte3 = (img_type >> 24) & 0xFF
                if img_type_byte3 != 2:
                    bin_name = decoded[2].rstrip(b'\t\r\n\0').decode()
                    for img_bin in bin_list:
                        if img_bin['name'] == bin_name:
                            img_bin['header_size'] = hdr_size
                            img_bin['image_size'] = img_size
                            img_bin['offset'] = offset
                            break

                if filesize < img_size:
                    SLOG_E("CODE_IMAGE_SIZE_NOT_MATCHED")
                    return CODE_IMAGE_SIZE_NOT_MATCHED

                offset = offset + img_size
                filesize = filesize - img_size
                src.seek(offset)

        # Verify Scan Result
        for img_bin in bin_list:
            if 'offset' not in img_bin:
                SLOG_W(img_bin['name'] + " is not found")
        return CODE_SUCCESS


    def __calc_hashtable_size(self, bundle_name, alignment):
        """
        Pre-Calc the size of Hashtable
        """
        bundle = self.m_bundle
        if bundle_name not in bundle:
            return CODE_DATA_INCORRECT

        hash_hdr_sz = struct.calcsize(SignImage.FLB_HASH_HEADER_PATTERN)
        item_count = len(bundle[bundle_name]) + 1  # Items and HDR Hash
        size = hash_hdr_sz
        size += struct.calcsize(SignImage.FLB_HASH_ITEM_PATTERN) * item_count
        block_sz = (size + (alignment - 1)) // alignment * alignment

        return CODE_SUCCESS, block_sz, size

    def __build_bundle_header(self, cfg, bundle_name, alignment, no_cert):
        """
        Build Bundle Image HDR
        """
        auth_type = self.m_common['auth_type']
        bundle = self.m_bundle
        if bundle_name not in bundle:
            return CODE_DATA_INCORRECT

        item_count = len(bundle[bundle_name])
        if item_count == 0:
            return CODE_DATA_INCORRECT

        if 'output' not in cfg or \
           'bundle_header' not in cfg['output'] or \
           'cert1' not in cfg['output']:
            return CODE_DATA_INCORRECT

        # Get Header File
        hdr_bin_file = cfg['output']['bundle_header']
        code = remove_file(hdr_bin_file)
        if code != CODE_SUCCESS:
            return code

        if not no_cert :
            # Get Cert1 File
            cert1_file = cfg['output']['cert1']
            code = check_file(cert1_file)
            if code != CODE_SUCCESS:
                return code

        # Get Security Level
        img_sec_level = bundle[bundle_name][0]['img_sec_level']
        sec_level = img_sec_level & 0xFF

        offset = 0
        item_count = len(bundle[bundle_name])
        flag = (1 << 1) # Certificate Chain
        if no_cert:
            item_count += 1  # Hashtable
        else:
            item_count += 3  # Hashtable + Cert1 + Cert2
            flag |= 1 # Enable Cert

        if auth_type:
            flag |=(1 << 5)
        else:
            flag |=(0 << 5)
        # Calc bundle header size
        hdr_size = struct.calcsize(SignImage.FLB_HEADER_PATTERN)
        hdr_size += (struct.calcsize(SignImage.FLB_ITEM_PATTERN) * item_count)
        hdr_block_sz = (hdr_size + (alignment - 1)) // alignment * alignment

        hdr_segment = dict()
        hdr_segment['size'] = hdr_size
        hdr_segment['block_size'] = hdr_block_sz
        hdr_segment['offset'] = offset
        offset += hdr_block_sz
        if not auth_type:
            # Calc image segments
            segments = []
            for img in bundle[bundle_name]:
                img_segment = dict()
                img_segment['name'] = img['name']
                img_segment['size'] = img['data_size']
                bsize = (img['data_size'] + (alignment - 1)) // alignment * alignment
                img_segment['block_size'] = bsize
                img_segment['offset'] = offset
                offset += bsize
                segments.append(img_segment)

            # Measure the size of hashtable
            code, hbsize, hsize = self.__calc_hashtable_size(bundle_name, alignment)
            if code != CODE_SUCCESS:
                return code

            hashtable = dict()
            hashtable['offset'] = offset
            hashtable['size'] = hsize
            hashtable['block_size'] = hbsize
            offset += hbsize

            if not no_cert:
                cert1_size = os.path.getsize(cert1_file)
                cert1_bsize = (cert1_size + (alignment - 1)) // alignment * alignment
                cert1_segment = dict()
                cert1_segment['offset'] = offset
                cert1_segment['size'] = cert1_size
                cert1_segment['block_size'] = cert1_bsize
                offset += cert1_segment['block_size']

                cert2_segment = dict()
                cert2_segment['offset'] = offset
                cert2_segment['size'] = cert1_bsize
                cert2_segment['block_size'] = cert1_bsize
                offset += cert2_segment['block_size']
        else:
            if not no_cert:
                cert1_size = os.path.getsize(cert1_file)
                cert1_bsize = (cert1_size + (alignment - 1)) // alignment * alignment
                cert1_segment = dict()
                cert1_segment['offset'] = offset
                cert1_segment['size'] = cert1_size
                cert1_segment['block_size'] = cert1_bsize
                offset += cert1_segment['block_size']

                cert2_segment = dict()
                cert2_segment['offset'] = offset
                cert2_segment['size'] = cert1_bsize
                cert2_segment['block_size'] = cert1_bsize
                offset += 0x800

            # Measure the size of hashtable
            code, hbsize, hsize = self.__calc_hashtable_size(bundle_name, alignment)
            if code != CODE_SUCCESS:
                return code

            hashtable = dict()
            hashtable['offset'] = offset
            hashtable['size'] = hsize
            hashtable['block_size'] = hbsize
            offset += hbsize
        
            # Calc image segments
            segments = []
            for img in bundle[bundle_name]:
                img_segment = dict()
                img_segment['name'] = img['name']
                img_segment['size'] = img['data_size']
                bsize = (img['data_size'] + (alignment - 1)) // alignment * alignment
                img_segment['block_size'] = bsize
                img_segment['offset'] = offset
                offset += bsize
                segments.append(img_segment)

        total_size = offset
        name = bundle_name.split(".")[0].strip()

        # Pack major header
        hdr = struct.pack(SignImage.FLB_HEADER_PATTERN,
                          SignImage.FLB_MAGIC,
                          int(total_size),
                          hdr_segment['block_size'],
                          alignment,
                          SignImage.FLB_HDR_VERSION,
                          name.encode(), item_count, flag)
        if not auth_type:
            # Pack segment headers
            for segment in segments:
                hdr += struct.pack(SignImage.FLB_ITEM_PATTERN,
                                   SignImage.FLB_SEGM_MAGIC,
                                   segment['name'].encode(),
                                   int(segment['offset']),
                                   int(segment['size']),
                                   int(segment['block_size']))

            # Pack hashtable segment
            hdr += struct.pack(SignImage.FLB_ITEM_PATTERN,
                               SignImage.FLB_SEGM_MAGIC,
                               b'authentication',
                               int(hashtable['offset']),
                               int(hashtable['size']),
                               int(hashtable['block_size']))

            # Pack certificates segment
            if not no_cert:
                hdr += struct.pack(SignImage.FLB_ITEM_PATTERN,
                                   SignImage.FLB_SEGM_MAGIC,
                                   b'cert1',
                                   int(cert1_segment['offset']),
                                   int(cert1_segment['size']),
                                   int(cert1_segment['block_size']))
                hdr += struct.pack(SignImage.FLB_ITEM_PATTERN,
                                   SignImage.FLB_SEGM_MAGIC,
                                   b'cert2',
                                   int(cert2_segment['offset']),
                                   int(cert2_segment['size']),
                                   0x800)

        else:
            # Pack certificates segment
            if not no_cert:
                hdr += struct.pack(SignImage.FLB_ITEM_PATTERN,
                                   SignImage.FLB_SEGM_MAGIC,
                                   b'cert1',
                                   int(cert1_segment['offset']),
                                   int(cert1_segment['size']),
                                   int(cert1_segment['block_size']))
                hdr += struct.pack(SignImage.FLB_ITEM_PATTERN,
                                   SignImage.FLB_SEGM_MAGIC,
                                   b'cert2',
                                   int(cert2_segment['offset']),
                                   int(cert2_segment['size']),
                                   0x800)
            # Pack hashtable segment
            hdr += struct.pack(SignImage.FLB_ITEM_PATTERN,
                               SignImage.FLB_SEGM_MAGIC,
                               b'authentication',
                               int(hashtable['offset']),
                               int(hashtable['size']),
                               int(hashtable['block_size']))
            # Pack segment headers
            for segment in segments:
                hdr += struct.pack(SignImage.FLB_ITEM_PATTERN,
                                   SignImage.FLB_SEGM_MAGIC,
                                   segment['name'].encode(),
                                   int(segment['offset']),
                                   int(segment['size']),
                                   int(segment['block_size']))


        # Add bundle header padding
        hdr += b'\x00' * (hdr_segment['block_size'] - hdr_segment['size'])
        with open(hdr_bin_file, 'wb') as dest:
            dest.write(hdr)

        del(hdr)
        del(segments)
        del(hashtable)
        del(hdr_segment)
        hdr = None
        segments = None
        hashtable = None
        hdr_segment = None

        return CODE_SUCCESS

    def __build_image_segments(self, bundle_name):
        """
        Build image segment blocks
        """
        bundle = self.m_bundle
        if bundle_name not in bundle:
            return CODE_DATA_INCORRECT

        out_dir = self.m_common['out_path']
        alignment = self.m_common['alignment']

        flashless_dir = os.path.join(out_dir, 'flashless')
        segment_dir = os.path.join(out_dir, 'flashless_segment')
        code = check_folder(segment_dir)
        if code != CODE_SUCCESS:
            return code

        for img_bin in bundle[bundle_name]:
            segment_src_file = os.path.join(flashless_dir, img_bin['name'])
            segment_dest_file = os.path.join(segment_dir, img_bin['name'])
            code = check_file(segment_src_file)
            if code != CODE_SUCCESS:
                return code
            code = remove_file(segment_dest_file)
            if code != CODE_SUCCESS:
                return code

            data_sz = img_bin['data_size']
            hdr_sz = img_bin['header_size']
            block_sz = (data_sz + (alignment - 1)) // alignment * alignment

            img_sec_level = img_bin['img_sec_level']
            sec_level = img_sec_level & 0xFF
            with open(segment_src_file, 'rb') as indata:
                # Move cursor to data block
                indata.seek(int(hdr_sz), 1)
                with open(segment_dest_file, "wb") as outdata:
                    sz = data_sz
                    block_len = 0
                    while(sz > 0):
                        if sz > 2048:
                            block_len = 2048
                        else:
                            block_len = sz

                        buf = indata.read(int(block_len))
                        outdata.write(buf)
                        sz -= block_len
                    outdata.write(b'\x00' * (int(block_sz) - int(data_sz)))

            flag = ''
            if sec_level == 0:
                flag = 'SHA256'
            elif sec_level == 1 or sec_level == 2 or sec_level == 3:
                flag = 'SHA384'
            else:
                return CODE_NOT_SUPPORT

            code, master = SignMaster.create()
            if code != CODE_SUCCESS:
                return code
            code, hashvalue = master.digest(flag, segment_dest_file)
            if code != CODE_SUCCESS:
                return code

            img_bin['segment'] = segment_dest_file
            img_bin['block_size'] = block_sz
            img_bin['hash'] = hashvalue

        return CODE_SUCCESS

    def __build_hash_and_cert2(self, cfg, bundle_name, alignment):
        """
        Build image segment blocks
        """
        bundle = self.m_bundle
        if bundle_name not in bundle:
            return CODE_DATA_INCORRECT

        if 'output' not in cfg:
            return CODE_DATA_INCORRECT
        if 'bundle_header' not in cfg['output']:
            return CODE_DATA_INCORRECT
        if 'hashtable' not in cfg['output']:
            return CODE_DATA_INCORRECT

        if 'stage2' not in cfg:
            return CODE_DATA_INCORRECT

        hdr_bin_file = cfg['output']['bundle_header']
        code = check_file(hdr_bin_file)
        if code != CODE_SUCCESS:
            return code

        hashtable_file = cfg['output']['hashtable']
        code = remove_file(hashtable_file)
        if code != CODE_SUCCESS:
            return code

        code, hblk_sz, hsz = self.__calc_hashtable_size(bundle_name, alignment)
        if code != CODE_SUCCESS:
            return code
        item_count = len(bundle[bundle_name]) + 1  # Items and HDR Hash

        settings = bundle[bundle_name][0]
        img_sec_level = settings['img_sec_level']
        sec_level = img_sec_level & 0xFF

        flag = ''
        flag2 = ''
        flag3 = ''
        algo = 0;
        algo_oid = ''
        if sec_level == 0:
            flag = 'SHA256'
            flag2 = 'RSA2048/SHA256'
            flag3 = 'RSA2048'
            algo = SignImage.ALG_ID_SHA256
            algo_oid = '1.2.840.113549.1.1.11'
        elif sec_level == 1:
            flag = 'SHA384'
            flag2 = 'RSA3072/SHA384'
            flag3 = 'RSA3072'
            algo = SignImage.ALG_ID_SHA384
            algo_oid = '1.2.840.113549.1.1.12'
        elif sec_level == 2:
            flag = 'SHA384'
            flag2 = 'RSA4096/SHA384'
            flag3 = 'RSA4096'
            algo = SignImage.ALG_ID_SHA384
            algo_oid = '1.2.840.113549.1.1.12'
        elif sec_level == 3:
            flag = 'SHA384'
            flag2 = 'EC384/SHA384'
            flag3 = 'EC384'
            algo = SignImage.ALG_ID_SHA384
            algo_oid = '1.2.840.10045.4.3.3'
        else:
            return CODE_SUCCESS

        code, master = SignMaster.create()
        if code != CODE_SUCCESS:
            return code

        raw = b''
        ht_size = 0
        with open(hashtable_file, 'wb') as dest:
            # Create Hashtable Header
            tb_hdr_sze = struct.calcsize(SignImage.FLB_HASH_HEADER_PATTERN)
            raw = struct.pack(SignImage.FLB_HASH_HEADER_PATTERN,
                              SignImage.FLB_HASH_MAGIC,
                              hblk_sz, tb_hdr_sze, alignment,
                              SignImage.FLB_HDR_VERSION, item_count)
            dest.write(raw)
            ht_size += len(raw)

            code, hashvalue = master.digest(flag, hdr_bin_file)
            if code != CODE_SUCCESS:
                return code

            # Append Hash of Header
            raw = struct.pack(SignImage.FLB_HASH_ITEM_PATTERN,
                              algo, len(hashvalue), hashvalue)
            dest.write(raw)
            ht_size += len(raw)

            for img in bundle[bundle_name]:
                # Append Hash of Image Segments
                raw = struct.pack(SignImage.FLB_HASH_ITEM_PATTERN,
                                  algo, len(img['hash']), img['hash'])
                dest.write(raw)
                ht_size += len(raw)

            ht_bsize = (ht_size + (alignment - 1)) // alignment * alignment
            dest.write(b'\x00' * (int(ht_bsize) - int(ht_size)))

        code, hash_of_hashtable = master.digest(flag, hashtable_file)
        if code != CODE_SUCCESS:
            return code

        config = cfg['stage1']
        cert_config = config['config_out']
        code = remove_file(cert_config)
        if code != CODE_SUCCESS:
            return code

        hash_path = config['hash_path']
        body_hash = os.path.join(hash_path, bundle_name + "_bhash.bin")
        code = remove_file(body_hash)
        if code != CODE_SUCCESS:
            return code

        with open(body_hash, 'wb') as hashout:
            hashout.write(hash_of_hashtable)

        shutil.copy2(config['config'], cert_config)

        img_key = settings['key']
        is_hsm = settings['hsm']
        sw_id = settings['sw_id']
        img_ver = settings['img_ver']
        img_group = settings['img_group']
        root_key_ver = settings['root_key_ver']
        socid = settings['socid']
        sig_pad = settings['sig_pad']

        replace_set = []
        replace_item = {}
        replace_item['pattern'] = SignImage.CERT_REPLACE_SIG_ALOG_OID
        replace_item['value'] = algo_oid
        replace_set.append(replace_item)

        if int(img_ver) != 0:
            replace_item = {}
            replace_item['value'] = str(img_ver)
            replace_item['pattern'] = SignImage.IMG_VER_REPLACE_TARGET
            replace_set.append(replace_item)
            del(replace_item)
        if int(img_sec_level) > 0:
            replace_item = {}
            replace_item['value'] = str(img_sec_level)
            replace_item['pattern'] = SignImage.SEC_LEVEL_REPLACE_TARGET
            replace_set.append(replace_item)
            del(replace_item)
        if socid is not None and socid != '':
            replace_item = {}
            replace_item['value'] = socid
            replace_item['pattern'] = SignImage.CERT2_REPLACE_SOCID
            replace_set.append(replace_item)
            del(replace_item)

        replace_item = {}
        replace_item['value'] = body_hash
        replace_item['pattern'] = SignImage.CERT2_REPLACE_HASH
        replace_set.append(replace_item)
        del(replace_item)

        pubk_path = config['pubk_path']
        code = remove_file(pubk_path)
        if code != CODE_SUCCESS:
            return code

        code, pubk = master.public_key(bool(is_hsm), img_key, flag3)
        if code != CODE_SUCCESS:
            return code
        code = master.pubk_to_pkcs8(flag3, pubk, pubk_path, None)
        if code != CODE_SUCCESS:
            return code

        replace_item = {}
        replace_item['value'] = pubk_path
        replace_item['pattern'] = SignImage.CERT1_REPLACE_IMG_PUBLIC_KEY1
        replace_set.append(replace_item)
        del(replace_item)

        code = self.__fill_cert_config(cert_config, replace_set)
        if code != CODE_SUCCESS:
            return code

        tbs_cert_path = config['tbs_cert_path']
        asn1_gen(cert_config, tbs_cert_path, False)

        out_path = config['bin_path']
        code = self.gen_cert(cfg, settings, 0, 'cert2', out_path, False, False)
        if code != CODE_SUCCESS:
            SLOG_E('GENERATE CERT2 FAILURE: ' + hex(code))
            return code

        return CODE_SUCCESS

    def __pack_bundle_image(self, cfg, bundle_name, alignment):
        """
        Pack segment into bundle
        """
        no_cert = self.m_common['no_cert']
        auth_type = self.m_common['auth_type']
        bundle = self.m_bundle
        if bundle_name not in bundle:
            return CODE_DATA_INCORRECT

        if 'output' not in cfg:
            return CODE_DATA_INCORRECT
        if 'bundle_header' not in cfg['output']:
            return CODE_DATA_INCORRECT
        if 'hashtable' not in cfg['output']:
            return CODE_DATA_INCORRECT
        if 'cert1' not in cfg['output']:
            return CODE_DATA_INCORRECT
        if 'cert2' not in cfg['output']:
            return CODE_DATA_INCORRECT

        if 'out_path' not in self.m_common:
            return CODE_DATA_INCORRECT

        out_path = self.m_common['out_path']
        no_cert = self.m_common['no_cert']
        fullname = os.path.join(out_path, bundle_name)
        code = remove_file(fullname)
        if code != CODE_SUCCESS:
            return code

        segment_dir = os.path.join(out_path, 'flashless_segment')
        hdr_file = cfg['output']['bundle_header']
        hashtable_file = cfg['output']['hashtable']
        cert1_file = cfg['output']['cert1']
        cert2_file = cfg['output']['cert2']

        pack_list = []
        hdr = dict()
        hdr['path'] = hdr_file
        hdr['padding'] = 0
        pack_list.append(hdr)

        ht = dict()
        ht['path'] = hashtable_file
        ht_sz = os.path.getsize(hashtable_file)
        ht['padding'] = 0

        if not auth_type:
            for segment in bundle[bundle_name]:
                segment_file = os.path.join(segment_dir, segment['name'])
                code = check_file(segment_file)
                if code != CODE_SUCCESS:
                    return code

                item = dict()
                item['path'] = segment_file
                size = os.path.getsize(segment_file)
                item['padding'] = 0
                pack_list.append(item)
            pack_list.append(ht)
            if not no_cert:
                cert1 = dict()
                cert1['path'] = cert1_file
                cert1_sz = os.path.getsize(cert1_file)
                cert1_blk_sz = (cert1_sz + (alignment - 1)) // alignment * alignment
                cert1['padding'] = cert1_blk_sz - cert1_sz
                pack_list.append(cert1)

                cert2 = dict()
                cert2['path'] = cert2_file
                cert2_sz = os.path.getsize(cert2_file)
                cert2_blk_sz = (cert2_sz + (alignment - 1)) // alignment * alignment
                cert2['padding'] = cert2_blk_sz - cert2_sz
                pack_list.append(cert2)
        else:
            if not no_cert:
                cert1 = dict()
                cert1['path'] = cert1_file
                cert1_sz = os.path.getsize(cert1_file)
                cert1_blk_sz = (cert1_sz + (alignment - 1)) // alignment * alignment
                cert1['padding'] = cert1_blk_sz - cert1_sz
                pack_list.append(cert1)

                cert2 = dict()
                cert2['path'] = cert2_file
                cert2_sz = os.path.getsize(cert2_file)
                cert2_blk_sz = (cert2_sz + (alignment - 1)) // alignment * alignment
                cert2['padding'] = cert2_blk_sz - cert2_sz
                pack_list.append(cert2)
            pack_list.append(ht)
            for segment in bundle[bundle_name]:
                segment_file = os.path.join(segment_dir, segment['name'])
                code = check_file(segment_file)
                if code != CODE_SUCCESS:
                    return code

                item = dict()
                item['path'] = segment_file
                size = os.path.getsize(segment_file)
                item['padding'] = 0
                pack_list.append(item)

        # Merge segment to file
        with open(fullname, 'wb') as dest:
            for item in pack_list:
                size = os.path.getsize(item['path'])
                with open(item['path'], 'rb') as src:
                    while size > 0:
                        block_sz = 2048
                        if size < 2048:
                            block_sz = size
                        raw = src.read(block_sz)
                        dest.write(raw)
                        size -= block_sz
                    if item['padding'] > 0:
                        dest.write(b'\x00' * item['padding'])

        SLOG_I("[OK] Pack Bundle: " + fullname)
        return CODE_SUCCESS

    def gen_tbs_cert2(self, image_name, cfg, properties, img_type):
        """Generate region of TBS_CERTIFICATE

        Args:
            image_name: image name of image
            cfg: environment setting to indentify the location of files
            properties: properties of the binary in image
            img_type: image type

        Returns:
            code: The return error message. 0 for success

        """
        config = cfg['stage1']
        dm_cert = config['dm_cert_der']
        image_file = config['image_file']

        bin_path = config['bin_path']
        hash_path = config['hash_path']

        bin_file = cfg['output']['bin']
        body_hash = os.path.join(hash_path, properties['name'] + "_bhash.bin")
        hdr_hash = os.path.join(hash_path, properties['name'] + "_hhash.bin")
        remove_file(bin_file)
        remove_file(body_hash)
        remove_file(hdr_hash)

        img_sec_level = properties['img_sec_level']
        sec_level = img_sec_level & 0xFF
        socid = properties['socid']

        offset = properties['offset']
        img_size = properties['image_size']
        hdr_size = properties['header_size']
        body_size = img_size - hdr_size

        has_dm_cert = False
        if img_type == IMG_TYPE_BOOTING:
            has_dm_cert = get_vboot10_cert(image_file, img_size, dm_cert)

        sig_pad = 'pss'
        if properties['sig_pad'] == '':
            sig_pad = 'legacy'
        else:
            sig_pad = properties['sig_pad']
        with open(image_file, 'rb') as src:
            with open(bin_file, 'wb') as dist:
                src.seek(offset)
                raw = src.read(int(img_size))
                dist.write(raw)

        hdr_file, body_file = self.__img_split(bin_file, bin_path, hdr_size)

        replace_set = []
        replace_item = {}
        replace_item['pattern'] = SignImage.CERT_REPLACE_SIG_ALOG_OID

        flag = ''
        flag2 = ''
        if sec_level == 0:
            flag = 'RSA2048'
            flag2 = 'SHA256'
            # sha256WithRSAEncryption
            replace_item['value'] = '1.2.840.113549.1.1.11'
        elif sec_level == 1:
            flag = 'RSA3072'
            flag2 = 'SHA384'
            # sha384WithRSAEncryption
            replace_item['value'] = '1.2.840.113549.1.1.12'
        elif sec_level == 2:
            flag = 'RSA4096'
            flag2 = 'SHA384'
            # sha384WithRSAEncryption
            replace_item['value'] = '1.2.840.113549.1.1.12'
        else:
            return CODE_NOT_SUPPORT

        replace_set.append(replace_item)

        code, master = SignMaster.create()
        if code != CODE_SUCCESS:
            return code
        code, hashvalue = master.digest(flag2, body_file)
        if os.path.getsize(body_file) == 0:
            hashvalue = b"\x00" * len(hashvalue)

        if code != CODE_SUCCESS:
            return code
        with open(body_hash, 'wb') as bhash:
            bhash.write(hashvalue)

        if hdr_size > 0:
            code, hashvalue = master.digest(flag2, hdr_file)
            if code != CODE_SUCCESS:
                return code
            with open(hdr_hash, 'wb') as bhash:
                bhash.write(hashvalue)

        if has_dm_cert:
            with open(dm_cert, 'rb') as dmcert:
                with open(bin_file, 'a') as binf:
                    binf.write(dmcert.read())

        cert_config = config['config_out']
        code = remove_file(cert_config)
        if code != CODE_SUCCESS:
            return code

        shutil.copy2(config['config'], cert_config)
        img_ver = properties['img_ver']
        img_sec_level = properties['img_sec_level']

        if int(img_ver) != 0:
            replace_item = {}
            replace_item['value'] = str(img_ver)
            replace_item['pattern'] = SignImage.IMG_VER_REPLACE_TARGET
            replace_set.append(replace_item)
            del(replace_item)
        if int(img_sec_level) > 0:
            replace_item = {}
            replace_item['value'] = str(img_sec_level)
            replace_item['pattern'] = SignImage.SEC_LEVEL_REPLACE_TARGET
            replace_set.append(replace_item)
            del(replace_item)
        if socid is not None and socid != '':
            replace_item = {}
            replace_item['value'] = socid
            replace_item['pattern'] = SignImage.CERT2_REPLACE_SOCID
            replace_set.append(replace_item)
            del(replace_item)

        replace_item = {}
        replace_item['value'] = body_hash
        replace_item['pattern'] = SignImage.CERT2_REPLACE_HASH
        replace_set.append(replace_item)
        del(replace_item)
        replace_item = {}
        replace_item['value'] = hdr_hash
        replace_item['pattern'] = SignImage.CERT2_REPLACE_HEADER_HASH
        replace_set.append(replace_item)
        del(replace_item)

        key = properties['key']
        is_hsm = properties['hsm']

        pubk_path = config['pubk_path']
        remove_file(pubk_path)

        code, pubk = master.public_key(bool(is_hsm), key, flag)
        if code != CODE_SUCCESS:
            return code
        code = master.pubk_to_pkcs8(flag, pubk, pubk_path, None)
        if code != CODE_SUCCESS:
            return code

        replace_item = {}
        replace_item['value'] = pubk_path
        replace_item['pattern'] = SignImage.CERT1_REPLACE_IMG_PUBLIC_KEY1
        replace_set.append(replace_item)
        del(replace_item)

        code = self.__fill_cert_config(cert_config, replace_set)
        if code != CODE_SUCCESS:
            return code

        tbs_cert_path = config['tbs_cert_path']
        asn1_gen(cert_config, tbs_cert_path, False)

        return CODE_SUCCESS

    def gen_all_flashless_cert2(self):
        """
        Generate flash-less bundle images
        """
        alignment = self.m_common['alignment']
        no_cert = self.m_common['no_cert']

        all_images = self.m_bundle
        for key in all_images:

            SLOG_I("Sign cert2 of " + key)
            code = self.__check_image_list(key)
            if code != CODE_SUCCESS:
                pass

            if len(all_images[key]) == 0 :
                SLOG_W('Length of ' + key +' images shall not be zero')
                continue

            img_type = 0
            bin_obj = all_images[key][0]
            bin_name = bin_obj['name']
            code, cfg = self.__gen_config(key, img_type, bin_name,
                                          'cert2', True)
            if code != CODE_SUCCESS:
                SLOG_E('GENERATE CERT2 FAILURE: ' + hex(code))
                return code

            # Extract image segments and compute hash values
            code = self.__build_image_segments(key)
            if code != CODE_SUCCESS:
                SLOG_W('GENERATE ' + key + ' segment fail: ' + hex(code))
                continue

            # Create bundle image header segment
            code = self.__build_bundle_header(cfg, key, alignment, no_cert)
            if code != CODE_SUCCESS:
                return code

            # Create hashtable
            code = self.__build_hash_and_cert2(cfg, key, alignment)
            if code != CODE_SUCCESS:
                return code

            # Combine segments
            code = self.__pack_bundle_image(cfg, key, alignment)
            if code != CODE_SUCCESS:
                return code
            SLOG_I('GENERATE ' + key + ' successfully')

        return CODE_SUCCESS

    def gen_all_cert2(self):
        """
        Generate cert2
        """
        if self.m_flashless:
            code = self.gen_all_flashless_cert2()
            return code

        all_images = dict()
        all_images.update(self.m_single_bin)
        all_images.update(self.m_multi_bin)
        all_images.update(self.m_image_hash_list)

        in_path = self.m_common['in_path']
        pack_list = dict()

        for key in all_images:
            is_squashfs = False
            if 'squashfs' in key:
                is_squashfs = True

            image = all_images[key]
            image_file = os.path.join(in_path, key)
            code, descriptor = image_descriptor(image_file, False, is_squashfs)
            if code != CODE_SUCCESS:
                SLOG_W("IMAGE NOT FOUND: " + image_file)
                continue

            img_type = descriptor['type']
            if img_type & IMG_TYPE_MK_MASK == 0:
                code = self.__scan_mkimage(image_file, image)
                if code != CODE_SUCCESS:
                    SLOG_E("Image: " + key + " is incorrect")
                    continue

            elif img_type == IMG_TYPE_BOOTING or \
                 img_type == IMG_TYPE_DTBO or \
                 img_type == IMG_TYPE_SQUASHFS:

                image[0]['offset'] = descriptor['offset']
                image[0]['header_size'] = descriptor['header_size']
                image[0]['image_size'] = descriptor['size']
            else:
                SLOG_E("Unsupported image type")
                return CODE_IMAGE_INVALID_IMAGE_TYPE

            for idx in range(0, len(image)):
                bin_obj = image[idx]
                bin_name = bin_obj['name']

                if 'offset' not in bin_obj:
                    SLOG_W(bin_name + 'is not found in ' + key)
                    continue

                isLast = False
                if idx == len(image) - 1:
                    isLast = True

                code, cfg = self.__gen_config(key, img_type, bin_name, 'cert2')
                if code != CODE_SUCCESS:
                    SLOG_E('GENERATE CERT2 FAILURE(1): ' + hex(code))
                    return code
                code = self.gen_tbs_cert2(key, cfg, bin_obj, img_type)
                if code != CODE_SUCCESS:
                    SLOG_E('GENERATE CERT2 FAILURE(2): ' + hex(code))
                    return code
                out_path = cfg['stage1']['bin_path']


                code = self.gen_cert(cfg, bin_obj, img_type, 'cert2', out_path,
                                     isLast)
                if code != CODE_SUCCESS:
                    SLOG_E('GENERATE CERT2 FAILURE(3): ' + hex(code))
                    return code

                if key not in pack_list:
                    pack_list[key] = dict()
                pack_list[key][bin_name] = cfg['output']

        for key in all_images:
            code = self.pack_image(key, all_images, pack_list)

        return CODE_SUCCESS

    @staticmethod
    def __extract_images(cfg, in_dir, out_dir):
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
            if "SBOOT_DIS" in filename:
                SLOG_W('IGNORE Secure DIS')
                continue
            filenames = filename.split(".")

            file = in_dir + "/" + filenames[0] + "-verified." + filenames[1]
            if not os.path.isfile(file):
                file = in_dir + "/" + filenames[0] + "." + filenames[1]
                if not os.path.isfile(file):
                    continue

            code, descriptor = image_descriptor(file, False)
            if code != CODE_SUCCESS:
                return CODE_EXECUTE_FAIL

            img_type = 0
            hdr_size = 0
            img_size = 0;
            img_offset = 0;
            if descriptor is None:
                img_type = IMG_TYPE_MK_RAW
                hdr_size = 0
            else:
                img_type = descriptor['type']
                hdr_size = descriptor['header_size']
                img_size = descriptor['size']
                img_offset = descriptor['offset']

            bin_items = cfg[filename]
            if len(bin_items) == 0:
                return CODE_INVALID_CONFIG_FILE

            if img_type & IMG_TYPE_MK_MASK == 0:
                with open(file, "rb") as indata:
                    while(1):
                        buf = indata.read(80)
                        if buf is None or buf == '' or len(buf) != 80:
                            break
                        data = struct.unpack("<I I 32s 12x I I I 4x I 8x", buf)
                        if data[0] == 0x58881688:
                            data_size = data[1]
                            item_file = (data[2].strip(b'\0')).decode()
                            hdr_size = data[3]
                            hdr_version = data[4]
                            hdr_type = data[5]
                            align = data[6]
                            MD_flag = None
                            if hdr_type > 0:
                                detail_type_byte0 = hdr_type & 0xFF
                                detail_type_byte3 = (hdr_type >> 24) & 0xFF
                                if detail_type_byte3 == 1:
                                    if detail_type_byte0 == 0:
                                        MD_flag = 'MD_LTE'
                                    elif detail_type_byte0 == 1:
                                        MD_flag = 'MD_CK2'

                            selected_item = None
                            for bin_item in bin_items:
                                if bin_item['name'] != item_file:
                                    continue
                                if 'bundle' not in bin_item or \
                                   bin_item['bundle'] == False:
                                    continue
                                selected_item = bin_item
                                break

                            if hdr_version != 1:
                                return CODE_INVALD_IMAGE_VER

                            totalsize = (data_size + hdr_size + align - 1) // \
                                        align * align

                            # Skip building image bin
                            if item_file == "cert1" or item_file == "cert2" or \
                               selected_item is None:
                                indata.seek(totalsize - 80, 1)
                            else:
                                binfile = out_dir + "/" + item_file
                                SLOG_I("outfile:" + binfile)
                                with open(binfile, "wb") as outdata:
                                    outdata.write(buf)
                                    buf = indata.read(int(totalsize) - 80)
                                    outdata.write(buf)
                                selected_item['MD'] = MD_flag
                                selected_item['header_size'] = hdr_size
                                selected_item['data_size'] = data_size

            elif img_type == IMG_TYPE_BOOTING:
                bin_name = None
                for bin_item in bin_items:
                    if 'boot' != bin_item['name']:
                        continue
                    if 'bundle' not in bin_item or \
                        bin_item['bundle'] == False:
                        continue
                    bin_name = bin_item['name']
                    bin_item['header_size'] = 0
                    bin_item['data_size'] = img_size
                    break

                if bin_name is not None:
                    binfile = out_dir + "/" + bin_name
                    SLOG_I("outfile:" + binfile)
                    with open(binfile, "wb") as outdata:
                        with open(file, "rb") as indata:
                            buf = indata.read(int(img_size))
                            outdata.write(buf)

        return CODE_SUCCESS

    def pack_image(self, name, all_images, pack_list):
        """
        Pack legacy image
        """
        out_path = self.m_common['out_path']
        is_squashfs = False
        if 'squashfs' in name:
            is_squashfs = True

        filename = ''
        if is_squashfs:
            image_name_split = name.split(".squashfs")
            filename = image_name_split[0] + "_ro.sig"
        else:
            image_name_split = name.split(".")
            filename = image_name_split[0] + "-verified." + image_name_split[1]

        fullname = os.path.join(out_path, filename)
        remove_file(fullname)
        image = all_images[name]

        if name not in pack_list:
            SLOG_W("IGNORE IMAGE: " + name)
            return CODE_SUCCESS

        bin_pack_list = pack_list[name]

        MAX_BUFFER_SIZE = 4096
        with open(fullname, 'wb') as dist:
            for bin_obj in image:
                if bin_obj['name'] in bin_pack_list:
                    cfg = pack_list[name][bin_obj['name']]
                    cert1 = cfg['cert1']
                    cert2 = cfg['cert2']
                    body = cfg['bin']
                    code = check_file(cert1)
                    if code != CODE_SUCCESS:
                        SLOG_E("CERT1 NOT FOUND, GEN CERT1 FIRST")
                        return code
                    code = check_file(cert2)
                    if code != CODE_SUCCESS:
                        LOG_E("CERT2 NOT FOUND, SYSTEM INCORRECT")
                        return code
                    code = check_file(body)
                    if code != CODE_SUCCESS:
                        LOG_E("BODY NOT FOUND, SYSTEM INCORRECT")
                        return code

                    if not is_squashfs:
                        size = os.path.getsize(body)
                        read_size = 0
                        with open(body, 'rb') as src:
                            while size > 0:
                                read_size = MAX_BUFFER_SIZE
                                if size < MAX_BUFFER_SIZE:
                                    read_size = size
                                raw = src.read(read_size)
                                dist.write(raw)
                                size = size - read_size
                    size = os.path.getsize(cert1)
                    read_size = 0

                    with open(cert1, 'rb') as src:
                        while size > 0:
                            read_size = MAX_BUFFER_SIZE
                            if size < MAX_BUFFER_SIZE:
                                read_size = size
                            raw = src.read(read_size)
                            dist.write(raw)
                            size = size - read_size
                    size = os.path.getsize(cert2)
                    read_size = 0

                    with open(cert2, 'rb') as src:
                        while size > 0:
                            read_size = MAX_BUFFER_SIZE
                            if size < MAX_BUFFER_SIZE:
                                read_size = size
                            raw = src.read(read_size)
                            dist.write(raw)
                            size = size - read_size

        SLOG_I("[OK] Pack: " + fullname)
        return CODE_SUCCESS

def fill_arg_dict(input_str, key, assign_key, args):
    """
    parse one input argument into input argument dictionary
    """
    prefix = input_str.split("=")[0]
    fmt = re.compile(key, re.I)
    if fmt.search(prefix):
        val = input_str.split("=")[1]
        args[key] = val

    return args

def parse_arg(argv):
    """
    parse one input arguments
    """
    argu_map = dict()
    argu_map['root_key_path'] = 'root_key_path'
    argu_map['oem_key_path'] = 'oem_key_path'
    argu_map['cert1_key_path'] = 'root_key_path'
    argu_map['cert2_key_path'] = 'oem_key_path'
    argu_map['env_cfg'] = 'env_cfg'
    argu_map['root_key_padding'] = 'root_key_padding'
    argu_map['hsm'] = 'is_hsm'
    argu_map['socid'] = 'socid'
    argu_map['target'] = 'target'

    if len(argv) < 4:
        SLOG_E("Function/Platform/Project is mandantory")
        return CODE_INVALID_INPUT, None

    args = dict()
    for input_str in argv:
        for key in argu_map:
            args = fill_arg_dict(input_str, key, argu_map[key], args)

    if argv[1] == '' or argv[2] == '':
        SLOG_E("Platform/Project is mandantory")
        return CODE_INVALID_INPUT, None

    board_avb_enable = os.environ.get('BOARD_AVB_ENABLE')
    if board_avb_enable is None or board_avb_enable != 'true':
        board_avb_enable = 'false'
    else:
        board_avb_enable = 'true'

    args['board_avb_enable'] = board_avb_enable
    for i in range(1, 3):
        if '=' in argv[i]:
            return CODE_INVALID_INPUT, None

    args['function'] = argv[1].strip()
    args['platform'] = argv[2].strip()
    args['project'] = argv[3].strip()

    if 'env_cfg' not in args or args['env_cfg'] == 0:
        # env_cfg is not given, we set it to env.cfg in path of this tool
        plat_env_dir = os.path.join(os.path.dirname(__file__), 'platform')
        plat_env_dir = os.path.join(plat_env_dir, args['platform'])
        plat_env_cfg = os.path.join(plat_env_dir, 'env.cfg')
        code = check_file(plat_env_cfg)
        if code == CODE_SUCCESS:
            args['env_cfg'] = plat_env_cfg
        else:
            args['env_cfg'] = os.path.join(os.path.dirname(__file__), 'env.cfg')

    return CODE_SUCCESS, args

def print_message():
    print ("\nsign_flow.py <all | image | img_cert_deploy> " + \
        "<platform> <project>\n" + \
        "\t [Optional] hsm=<0: No, 1: Yes>\n" + \
        "\t [Optional] root_key_path=<key_path>\n" + \
        "\t [Optional] oem_key_path=<key_path>\n" + \
        "\t [Optional] root_key_padding=<padding_type, pss>\n" + \
        "\t [Optional] socid=<socid>\n")

def main():
    """
    main function, which is executed when this is executed from cmdline.
    """
    code, args = parse_arg(sys.argv)
    if code == CODE_SUCCESS:
        fn = args['function'].strip()
        if 'all' != fn and 'image' != fn and 'img_cert_deploy' != fn:

            code = CODE_INVALID_INPUT

    if code != CODE_SUCCESS:
        print_message()
        sys.exit(code)
        return

    is_hsm = '0'
    target = None
    iv = dict()
    if 'is_hsm' in args:
        iv['hsm'] = args['is_hsm']
    if 'oem_key_path' in args:
        iv['key'] = args['oem_key_path']
    if 'root_key_path' in args:
        iv['root_key'] = args['root_key_path']
    if 'root_key_padding' in args:
        iv['sig_pad'] = args['root_key_padding']
    if 'socid' in args:
        iv['socid'] = args['socid']
    if 'target' in args:
        iv['target'] = args['target']

    iv['board_avb_enable'] = args['board_avb_enable']
    config = args['env_cfg']
    platform = args['platform']
    project = args['project']

    if fn == 'image':
        iv['root_key_verification'] = False
    else:
        iv['root_key_verification'] = True

    code, si = SignImage.create(config, platform, project, iv)
    if code != CODE_SUCCESS:
        print_message()
        sys.exit(code)
        return

    if 'all' == fn or 'img_cert_deploy' == fn:
        code = si.gen_all_cert1()
        if code != CODE_SUCCESS:
            print_message()
            sys.exit(code)
            return

    if 'all' == fn or 'image' == fn:
        code = si.gen_all_cert2()
        if code != CODE_SUCCESS:
            print_message()
            sys.exit(code)
            return

    SLOG_I("GENERATION COMPLETE !!!")

if __name__ == '__main__':
    main()

