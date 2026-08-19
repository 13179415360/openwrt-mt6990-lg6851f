"""
This module is responsible for Preloader image signing
Security Sensitive!!! Never Release To Customers In Source Form!!!
Example:
    [SEC_LEVEL_0]
    python pbpv2.py -g settings/pbp/sec_level_0/pl_gfh_config_cert_chain.ini
        -i settings/pbp/sec_level_0/pl_key.ini
        -c settings/pbp/sec_level_0/pl_content.ini
        -func sign
        -o out/my_2048_preloader.bin testresults/preloader_evb6890v1_64_cpe.bin
    [SEC_LEVEL_1]
    python pbpv2.py -g settings/pbp/sec_level_1/pl_gfh_config_cert_chain.ini
        -i settings/pbp/sec_level_1/pl_key.ini
        -c settings/pbp/sec_level_1/pl_content.ini
        -func sign
        -o out/my_3072_preloader.bin testresults/preloader_evb6890v1_64_cpe.bin
    [SEC_LEVEL_2]
    python pbpv2.py -g settings/pbp/sec_level_2/pl_gfh_config_cert_chain.ini
        -i settings/pbp/sec_level_2/pl_key.ini
        -c settings/pbp/sec_level_2/pl_content.ini
        -func sign
        -o out/my_4096_preloader.bin testresults/preloader_evb6890v1_64_cpe.bin
"""

__copyright__ = "Copyright 2017-2022, MediaTek Inc."
__version__ = "3.0.1"

import argparse
import os
import struct
import sys
import traceback

from lib.cert import CertificateChain
from lib.cert import SingleCertificate
from lib.cert import KeyCert

from lib.gfh import ImageGFH

from lib.sign_error import CODE_SUCCESS
from lib.sign_error import CODE_NO_GFH_HEADER
from lib.sign_error import CODE_NOT_SUPPORT
from lib.sign_error import CODE_INVALID_INPUT
from lib.sign_error import CODE_ILIEGAL_STATE
from lib.sign_error import CODE_CERT_TYPE_MISMATCHED

from lib.sign_master import SignMaster

from lib.sign_util import check_file
from lib.sign_util import check_folder
from lib.sign_util import remove_file
from lib.sign_util import load_config
from lib.sign_util import SLOG_D
from lib.sign_util import SLOG_E
from lib.sign_util import SLOG_I

DEBUG = False

class RMA:
    STATE_INIT       = 0
    STATE_LOAD_INI   = 1
    STATE_SIGNED     = 2
    STATE_PACKED     = 3

    def __init__(self, name):
        self.m_name = name
        self.m_master = None
        self.m_gfh_handler = None

        self.m_sec_level = 0
        self.m_sig_type = ''
        self.m_pad_type = ''
        self.m_hdr_len = 0
        self.m_img_len = 0
        self.m_sig_len = 0
        self.m_sig_ver = 0
        self.m_body_alignment = 0
        self.m_state = RMA.STATE_INIT

        self.m_sig_handler = None
        self.m_intermediate_dir = ''
        self.m_image_file = ''
        self.m_signature_file = ''

    @staticmethod
    def create(name, intermediate_dir):
        """Create the RMA object after checking intermediate folder and
           create the instance of SignMaster. The BL creating will create
           ImageGFH at the same time
        Args:
            name (string): RMA name, just for identification
            intermediate_dir (string): Temp folder for intermediate files
        Return:
            code (int): Status code of execution
            RMA (object): Instance of RMA class
        """
        code = check_folder(intermediate_dir)
        if code != CODE_SUCCESS:
            return code, None

        code, master = SignMaster.create()
        if code != CODE_SUCCESS:
            return code, None

        rma = RMA(name)
        rma.m_master = master
        rma.m_intermediate_dir = intermediate_dir
        rma.m_gfh_handler = ImageGFH(master, intermediate_dir)

        return CODE_SUCCESS, rma

    def load_ini(self, gfh_ini_config_path, cert_cfg_files):
        """Load configuration from different file, depend on the signature type
           defined in GFH configuration file
        Args:
            gfh_ini_config_path (string): Path of GFH configuration file
            cert_cfg_files (string list): Paths of certificate configurations
        Return:
            code (int): Status code of execution
        """
        if RMA.STATE_INIT != self.m_state:
            return CODE_ILIEGAL_STATE

        # LOAD INI dictionary to GFH handlers
        gfh_handler = self.m_gfh_handler
        code = gfh_handler.load_ini(gfh_ini_config_path, cert_cfg_files[0])
        if code != CODE_SUCCESS:
            return code

        self.m_sec_level = gfh_handler.sec_level()
        self.m_sig_type = gfh_handler.sig_type()
        self.m_pad_type = gfh_handler.pad_type()
        SLOG_D("SEC_LEVEL: " + str(self.m_sec_level))
        SLOG_D("SIG_TYPE: " + str(self.m_sig_type))
        SLOG_D("PAD_TYPE: " + str(self.m_pad_type))

        if 'NONE' == self.m_sig_type or 'NONE' == self.m_pad_type:
            return CODE_INVALID_INPUT

        if 'CERT_CHAIN' == self.m_sig_type:
            code, handler = CertificateChain.load_ini(self.m_master,
                                                      cert_cfg_files,
                                                      self.m_pad_type,
                                                      self.m_sec_level)
            if code != CODE_SUCCESS:
                return code
            sig_len = handler.size()
            SLOG_D("SIG len: " + str(sig_len))

            code = gfh_handler.set_size(self.m_img_len, sig_len)
            if code != CODE_SUCCESS:
                return code

            self.m_body_pad_size = gfh_handler.body_pad_size()
            SLOG_D("BODY PADDING SIZE: " + str(self.m_body_pad_size))

            self.m_sig_handler = handler
        else:
            return CODE_CERT_TYPE_MISMATCHED

        self.m_state = RMA.STATE_LOAD_INI
        return CODE_SUCCESS

    def sign(self, key_cert_path):
        """Sign Bootloader and GFH

        Args:
            None
        Return:
            code (int): Status code of execution
        """
        if self.m_state != RMA.STATE_LOAD_INI:
            return CODE_ILIEGAL_STATE

        if self.m_intermediate_dir == '':
            return CODE_ILIEGAL_STATE

        intermediate_dir = self.m_intermediate_dir
        img_file_w_GFH = os.path.join(intermediate_dir, "RMA_W_GFH.bin")
        code = remove_file(img_file_w_GFH)
        if code != CODE_SUCCESS:
            return code

        code, header = self.m_gfh_handler.pack()
        if code != CODE_SUCCESS:
            SLOG_E("Pack GFH Failure: " + hex(code))
            return code

        # Pack Signed BL Image
        with open(img_file_w_GFH, 'wb') as unsigned_rma:
            # GFG Header
            unsigned_rma.write(header)
            # NO GFH Image
            padding_size = self.m_body_pad_size
            unsigned_rma.write(b'\0' * padding_size)

        signature_file = os.path.join(intermediate_dir, "RMA_SIG.bin")
        code = remove_file(signature_file)
        if code != CODE_SUCCESS:
            return code

        if self.m_sig_handler is None:
            return CODE_ILIEGAL_STATE

        code = self.m_sig_handler.sign(img_file_w_GFH, intermediate_dir)
        if code != CODE_SUCCESS:
            SLOG_E("Signature failure: " + hex(code))
            return code

        self.m_image_file = img_file_w_GFH

        rep = None
        if key_cert_path is not None:
            rep = dict()
            rep[KeyCert.NAME] = key_cert_path


        code = self.m_sig_handler.pack(signature_file, intermediate_dir, rep)
        if code != CODE_SUCCESS:
            SLOG_E("Pack Signature failure: " + hex(code))
            return code

        self.m_signature_file = signature_file
        self.m_state = BL.STATE_SIGNED
        return CODE_SUCCESS

    def pack(self, signed_bl_file):
        """Pack signed bootloader

        Args:
            signed_bl_file (string): Path of output signed bootloader
        Return:
            code (int): Status code of execution
        """
        if self.m_state != BL.STATE_SIGNED:
            return CODE_ILIEGAL_STATE
        if self.m_image_file == '' or self.m_signature_file == '':
            return CODE_ILIEGAL_STATE
        code = remove_file(signed_bl_file)
        if code != CODE_SUCCESS:
            return code

        # Pack Signed BL Image
        with open(signed_bl_file, 'wb') as signed_bl:
            # Image w/ GFH
            size = os.path.getsize(self.m_image_file)
            with open(self.m_image_file, 'rb') as img:
                while size > 0:
                    block_sz = 2048
                    if size < 2048:
                        block_sz = size
                    raw = img.read(block_sz)
                    signed_bl.write(raw)
                    size -= block_sz
            remove_file(self.m_image_file)
            self.m_image_file = ''
            # Signature
            size = os.path.getsize(self.m_signature_file)
            with open(self.m_signature_file, 'rb') as sig:
                while size > 0:
                    block_sz = 2048
                    if size < 2048:
                        block_sz = size
                    raw = sig.read(block_sz)
                    signed_bl.write(raw)
                    size -= block_sz
            remove_file(self.m_signature_file)
            self.m_signature_file = ''

        self.m_state = BL.STATE_PACKED
        return CODE_SUCCESS

class TOOLAUTH:
    STATE_INIT       = 0
    STATE_LOAD_INI   = 1
    STATE_SIGNED     = 2
    STATE_PACKED     = 3

    def __init__(self, name):
        self.m_name = name
        self.m_master = None
        self.m_gfh_handler = None

        self.m_sec_level = 0
        self.m_sig_type = ''
        self.m_pad_type = ''
        self.m_hdr_len = 0
        self.m_img_len = 0
        self.m_sig_len = 0
        self.m_sig_ver = 0
        self.m_body_alignment = 0
        self.m_state = TOOLAUTH.STATE_INIT

        self.m_sig_handler = None
        self.m_intermediate_dir = ''
        self.m_image_file = ''
        self.m_signature_file = ''

    @staticmethod
    def create(name, intermediate_dir):
        """Create the TOOLAUTH object after checking intermediate folder and
           create the instance of SignMaster. The BL creating will create
           ImageGFH at the same time
        Args:
            name (string): Toolauth name, just for identification
            intermediate_dir (string): Temp folder for intermediate files
        Return:
            code (int): Status code of execution
            TOOLAUTH (object): Instance of TOOLAUTH class
        """
        code = check_folder(intermediate_dir)
        if code != CODE_SUCCESS:
            return code, None

        code, master = SignMaster.create()
        if code != CODE_SUCCESS:
            return code, None

        toolauth = TOOLAUTH(name)
        toolauth.m_master = master
        toolauth.m_intermediate_dir = intermediate_dir
        toolauth.m_gfh_handler = ImageGFH(master, intermediate_dir)

        return CODE_SUCCESS, toolauth

    def load_ini(self, gfh_ini_config_path, cert_cfg_files):
        """Load configuration from different file, depend on the signature type
           defined in GFH configuration file
        Args:
            gfh_ini_config_path (string): Path of GFH configuration file
            cert_cfg_files (string list): Paths of certificate configurations
        Return:
            code (int): Status code of execution
        """
        if TOOLAUTH.STATE_INIT != self.m_state:
            return CODE_ILIEGAL_STATE

        # LOAD INI dictionary to GFH handlers
        gfh_handler = self.m_gfh_handler
        code = gfh_handler.load_ini(gfh_ini_config_path, cert_cfg_files[0])
        if code != CODE_SUCCESS:
            return code

        self.m_sec_level = gfh_handler.sec_level()
        self.m_sig_type = gfh_handler.sig_type()
        self.m_pad_type = gfh_handler.pad_type()
        SLOG_D("SEC_LEVEL: " + str(self.m_sec_level))
        SLOG_D("SIG_TYPE: " + str(self.m_sig_type))
        SLOG_D("PAD_TYPE: " + str(self.m_pad_type))

        if 'NONE' == self.m_sig_type or 'NONE' == self.m_pad_type:
            return CODE_INVALID_INPUT

        if 'SINGLE' == self.m_sig_type:
            code, config = load_config(cert_cfg_files[0])
            if code != CODE_SUCCESS:
                return code
            code, handler = SingleCertificate.load_ini(self.m_master,
                                                       config,
                                                       self.m_pad_type,
                                                       self.m_sec_level)
            if code != CODE_SUCCESS:
                return code            
            sig_len = int(handler.size())
            gfh_handler.set_size(0, sig_len)
            SLOG_D("SIG len: " + str(sig_len))
            self.m_sig_handler = handler
        else:
            return CODE_CERT_TYPE_MISMATCHED

        self.m_state = TOOLAUTH.STATE_LOAD_INI
        return CODE_SUCCESS

    def sign(self):
        """Sign Toolauth and GFH

        Args:
            None
        Return:
            code (int): Status code of execution
        """
        if self.m_state != TOOLAUTH.STATE_LOAD_INI:
            return CODE_ILIEGAL_STATE

        if self.m_intermediate_dir == '':
            return CODE_ILIEGAL_STATE

        intermediate_dir = self.m_intermediate_dir
        img_file_w_GFH = os.path.join(intermediate_dir, "TOOLAUTH_W_GFH.bin")
        code = remove_file(img_file_w_GFH)
        if code != CODE_SUCCESS:
            return code

        code, header = self.m_gfh_handler.pack()
        if code != CODE_SUCCESS:
            SLOG_E("Pack GFH Failure: " + hex(code))
            return code

        # Pack Signed BL Image
        with open(img_file_w_GFH, 'wb') as unsigned_bl:
            # GFG Header
            unsigned_bl.write(header)
            # TOOLAUTH NO IMAGE

        signature_file = os.path.join(intermediate_dir, "TOOLAUTH_SIG.bin")
        code = remove_file(signature_file)
        if code != CODE_SUCCESS:
            return code

        if self.m_sig_handler is None:
            return CODE_ILIEGAL_STATE

        code = self.m_sig_handler.sign(img_file_w_GFH)
        if code != CODE_SUCCESS:
            SLOG_E("Signature failure: " + hex(code))
            return code

        self.m_image_file = img_file_w_GFH

        code = self.m_sig_handler.pack(signature_file)
        if code != CODE_SUCCESS:
            SLOG_E("Pack Signature failure: " + hex(code))
            return code

        self.m_signature_file = signature_file
        self.m_state = TOOLAUTH.STATE_SIGNED
        return CODE_SUCCESS

    def pack(self, signed_ta_file):
        """Pack signed toolauth

        Args:
            signed_ta_file (string): Path of output signed toolauth
        Return:
            code (int): Status code of execution
        """
        if self.m_state != TOOLAUTH.STATE_SIGNED:
            return CODE_ILIEGAL_STATE
        if self.m_image_file == '' or self.m_signature_file == '':
            return CODE_ILIEGAL_STATE
        code = remove_file(signed_ta_file)
        if code != CODE_SUCCESS:
            return code

        # Pack Signed BL Image
        with open(signed_ta_file, 'wb') as signed_toolauth:
            # Image w/ GFH
            size = os.path.getsize(self.m_image_file)
            with open(self.m_image_file, 'rb') as img:
                while size > 0:
                    block_sz = 2048
                    if size < 2048:
                        block_sz = size
                    raw = img.read(block_sz)
                    signed_toolauth.write(raw)
                    size -= block_sz
            remove_file(self.m_image_file)
            self.m_image_file = ''
            # Signature
            size = os.path.getsize(self.m_signature_file)
            with open(self.m_signature_file, 'rb') as sig:
                while size > 0:
                    block_sz = 2048
                    if size < 2048:
                        block_sz = size
                    raw = sig.read(block_sz)
                    signed_toolauth.write(raw)
                    size -= block_sz
            remove_file(self.m_signature_file)
            self.m_signature_file = ''

        self.m_state = TOOLAUTH.STATE_PACKED
        return CODE_SUCCESS

class BL:
    STATE_INIT       = 0
    STATE_LOAD_IMAGE = 1
    STATE_LOAD_INI   = 2
    STATE_SIGNED     = 3
    STATE_PACKED     = 4

    def __init__(self, name):
        self.m_name = name
        self.m_master = None
        self.m_gfh_handler = None

        self.m_sec_level = 0
        self.m_sig_type = ''
        self.m_pad_type = ''
        self.m_hdr_len = 0
        self.m_img_len = 0
        self.m_sig_len = 0
        self.m_sig_ver = 0
        self.m_body_alignment = 0
        self.m_state = BL.STATE_INIT

        self.m_sig_handler = None
        self.m_intermediate_dir = ''
        self.m_image_file = ''
        self.m_signature_file = ''

    @staticmethod
    def create(name, intermediate_dir):
        """Create the BL object after checking intermediate folder and create
           the instance of SignMaster. The BL creating will create ImageGFH at
           the same time
        Args:
            name (string): Bootloader name, just for identification
            intermediate_dir (string): Temp folder for intermediate files
        Return:
            code (int): Status code of execution
            BL (object): Instance of BL class
        """
        code = check_folder(intermediate_dir)
        if code != CODE_SUCCESS:
            return code, None

        code, master = SignMaster.create()
        if code != CODE_SUCCESS:
            return code, None

        bootloader = BL(name)
        bootloader.m_master = master
        bootloader.m_gfh_handler = ImageGFH(master, intermediate_dir)
        bootloader.m_intermediate_dir = intermediate_dir
        return CODE_SUCCESS, bootloader

    def load(self, image_file):
        """Load external bootloadr image to intermediate folder, remove the
           GFH header and signature segment if the image is already signed
        Args:
            image_file (string): Path of external bootloader image
        Return:
            code (int): Status code of execution
        """
        code = check_file(image_file)
        if code != CODE_SUCCESS:
            SLOG_E("LOAD IMAGE FILE FAILURE: " + image_file)
            return code

        intermediate_dir = self.m_intermediate_dir
        no_GFH_file = os.path.join(intermediate_dir, "PRELOADER_NOGFH.bin")
        code = remove_file(no_GFH_file)
        if code != CODE_SUCCESS:
            SLOG_E("REMOVE NO GFH IMAGE FAIURE: " + image_file)
            return code

        code = ImageGFH.extract_image(image_file, no_GFH_file)
        if code != CODE_SUCCESS:
            return code

        self.m_img_len = os.path.getsize(no_GFH_file)
        self.m_image_file = no_GFH_file
        self.m_state = BL.STATE_LOAD_IMAGE
        return CODE_SUCCESS

    def load_ini(self, gfh_ini_config_path, cert_cfg_files):
        """Load configuration from different file, depend on the signature type
           defined in GFH configuration file
        Args:
            gfh_ini_config_path (string): Path of GFH configuration file
            cert_cfg_files (string list): Paths of certificate configurations
        Return:
            code (int): Status code of execution
        """
        if BL.STATE_LOAD_IMAGE != self.m_state:
            return CODE_ILIEGAL_STATE

        # LOAD INI dictionary to GFH handlers
        gfh_handler = self.m_gfh_handler
        code = gfh_handler.load_ini(gfh_ini_config_path, cert_cfg_files[0])
        if code != CODE_SUCCESS:
            return code

        self.m_sec_level = gfh_handler.sec_level()
        self.m_sig_type = gfh_handler.sig_type()
        self.m_pad_type = gfh_handler.pad_type()
        if 'NONE' == self.m_sig_type or 'NONE' == self.m_pad_type:
            return CODE_INVALID_INPUT

        if 'CERT_CHAIN' == self.m_sig_type:
            code, handler = CertificateChain.load_ini(self.m_master,
                                                      cert_cfg_files,
                                                      self.m_pad_type,
                                                      self.m_sec_level)
            if code != CODE_SUCCESS:
                return code
            sig_len = handler.size()
            SLOG_D("SIG len: " + str(sig_len))

            code = gfh_handler.set_size(self.m_img_len, sig_len)
            if code != CODE_SUCCESS:
                return code

            self.m_body_pad_size = gfh_handler.body_pad_size()
            SLOG_D("BODY PADDING SIZE: " + str(self.m_body_pad_size))

            self.m_sig_handler = handler
        else:
            return CODE_NOT_SUPPORT

        self.m_state = BL.STATE_LOAD_INI
        return CODE_SUCCESS

    def sign(self, key_cert_path):
        """Sign Bootloader and GFH

        Args:
            None
        Return:
            code (int): Status code of execution
        """
        if self.m_state != BL.STATE_LOAD_INI:
            return CODE_ILIEGAL_STATE

        if self.m_image_file == '' or self.m_intermediate_dir == '':
            return CODE_ILIEGAL_STATE

        intermediate_dir = self.m_intermediate_dir
        img_file_w_GFH = os.path.join(intermediate_dir, "PRELOADER_W_GFH.bin")
        code = remove_file(img_file_w_GFH)
        if code != CODE_SUCCESS:
            return code

        code, header = self.m_gfh_handler.pack()
        if code != CODE_SUCCESS:
            SLOG_E("Pack GFH Failure: " + hex(code))
            return code

        # Pack Signed BL Image
        with open(img_file_w_GFH, 'wb') as unsigned_bl:
            # GFG Header
            unsigned_bl.write(header)
            # NO GFH Image
            size = self.m_img_len
            with open(self.m_image_file, 'rb') as img:
                while size > 0:
                    block_sz = 2048
                    if size < 2048:
                        block_sz = size
                    raw = img.read(block_sz)
                    unsigned_bl.write(raw)
                    size -= block_sz
                padding_size = self.m_body_pad_size
                unsigned_bl.write(b'\0' * padding_size)

        signature_file = os.path.join(intermediate_dir, "PRLOADER_SIG.bin")
        code = remove_file(signature_file)
        if code != CODE_SUCCESS:
            return code

        if self.m_sig_handler is None:
            return CODE_ILIEGAL_STATE

        code = self.m_sig_handler.sign(img_file_w_GFH, intermediate_dir)
        if code != CODE_SUCCESS:
            SLOG_E("Signature failure: " + hex(code))
            return code

        SLOG_D("NO GFH: " + self.m_image_file)
        remove_file(self.m_image_file)
        self.m_image_file = img_file_w_GFH

        rep = None
        if key_cert_path is not None:
            rep = dict()
            rep[KeyCert.NAME] = key_cert_path

        code = self.m_sig_handler.pack(signature_file, intermediate_dir, rep)
        if code != CODE_SUCCESS:
            SLOG_E("Pack Signature failure: " + hex(code))
            return code

        self.m_signature_file = signature_file
        self.m_state = BL.STATE_SIGNED
        return CODE_SUCCESS

    def pack(self, signed_bl_file):
        """Pack signed bootloader

        Args:
            signed_bl_file (string): Path of output signed bootloader
        Return:
            code (int): Status code of execution
        """
        if self.m_state != BL.STATE_SIGNED:
            return CODE_ILIEGAL_STATE
        if self.m_image_file == '' or self.m_signature_file == '':
            return CODE_ILIEGAL_STATE
        code = remove_file(signed_bl_file)
        if code != CODE_SUCCESS:
            return code

        # Pack Signed BL Image
        with open(signed_bl_file, 'wb') as signed_bl:
            # Image w/ GFH
            size = os.path.getsize(self.m_image_file)
            with open(self.m_image_file, 'rb') as img:
                while size > 0:
                    block_sz = 2048
                    if size < 2048:
                        block_sz = size
                    raw = img.read(block_sz)
                    signed_bl.write(raw)
                    size -= block_sz
            remove_file(self.m_image_file)
            self.m_image_file = ''

            size = os.path.getsize(self.m_signature_file)
            sig_sz_path = os.path.join(self.m_intermediate_dir, "sig_size.txt")
            with open(sig_sz_path, 'w') as dest:
                dest.write(str(size))

            # Signature
            size = os.path.getsize(self.m_signature_file)
            with open(self.m_signature_file, 'rb') as sig:
                while size > 0:
                    block_sz = 2048
                    if size < 2048:
                        block_sz = size
                    raw = sig.read(block_sz)
                    signed_bl.write(raw)
                    size -= block_sz
            remove_file(self.m_signature_file)
            self.m_signature_file = ''

        self.m_state = BL.STATE_PACKED
        return CODE_SUCCESS

class PbpArgs(object):
    """
    PbpArgs is used to pass parameter to pbp.
    This structure is both used when user executes this python script directly or imports
    this module and use exported method.
    """
    def __init__(self):
        self.op = None
        self.padding = None
        self.key_ini_path = None
        self.key_path = None
        self.gfh_cfg_ini_path = None
        self.cnt_cfg_ini_path = None
        self.key_cert_path = None
        self.input_bl_path = None
        self.tmp_output_path = None
        self.output_path = None
    def reset(self):
        self.__init__()
    def dump(self):
        """
        dump parameters.
        """
        f = lambda arg: 'Not Set' if arg is None else arg
        print ("op = " + f(self.op))
        print ("padding = " + f(self.padding))
        print ("key_ini_path = " + f(self.key_ini_path))
        print ("key_path = " + f(self.key_path))
        print ("gfh_cfg_ini_path = " + f(self.gfh_cfg_ini_path))
        print ("cnt_cfg_ini_path = " + f(self.cnt_cfg_ini_path))
        print ("key_cert_path = " + f(self.key_cert_path))
        print ("input_bl_path = " + f(self.input_bl_path))
        print ("tmp_output_path = " + f(self.tmp_output_path))
        print ("output_path = " + f(self.output_path))

def _op_sign_toolauth(args):
    """
    Sign/re-sign toolauth operation
    """
    if args.intermediate_dir:
        intermediate_dir = args.intermediate_dir
    else:
        intermediate_dir = args.tmp_output_path

    code, toolauth = TOOLAUTH.create("Toolauth", intermediate_dir)
    if code != CODE_SUCCESS:
        SLOG_E("Create BL failure: " + hex(code))
        return code

    gfh_ini = args.gfh_cfg_ini_path
    certs_ini = [args.key_ini_path]
    signed_toolauth_file = args.output_path

    code = check_file(gfh_ini)
    if code != CODE_SUCCESS:
        SLOG_E("GFH INI file is not found")
        return code

    if certs_ini is None or len(certs_ini) != 1:
        SLOG_E("Key INI file are not set")

    code = check_file(certs_ini[0])
    if code != CODE_SUCCESS:
        SLOG_E("KEY INI file is not found")
        return code

    code = check_folder(intermediate_dir)
    if code != CODE_SUCCESS:
        SLOG_E("Intermediate folder is not found")
        return code

    SLOG_I("====== SIGN TOOLAUTH ======")
    SLOG_I("OUT File: " + os.path.realpath(signed_toolauth_file))
    SLOG_I("Intermediate DIR: " + os.path.realpath(intermediate_dir))
    SLOG_I("                            ")

    code = toolauth.load_ini(gfh_ini, certs_ini)
    if code != CODE_SUCCESS:
        SLOG_E("Load INI failure: " + hex(code))
        return code

    code = toolauth.sign()
    if code != CODE_SUCCESS:
        SLOG_E("Sign failure: " + hex(code))
        return code

    code = toolauth.pack(signed_toolauth_file)
    if code != CODE_SUCCESS:
        SLOG_E("Pack failure: " + hex(code))
        return code

    signed_toolauth_file = os.path.realpath(signed_toolauth_file)
    SLOG_I("Encoded Signed TOOLAUTH successfully, please check content")
    SLOG_I("CHECK: " + signed_toolauth_file)
    return CODE_SUCCESS

def _op_sign_bl(args):
    """
    Sign/re-sign preloader operation
    """
    if args.intermediate_dir:
        intermediate_dir = args.intermediate_dir
    else:
        intermediate_dir = args.tmp_output_path

    code, bl = BL.create("Preloader", intermediate_dir)
    if code != CODE_SUCCESS:
        SLOG_E("Create BL failure: " + hex(code))
        return code

    key_cert_path = args.key_cert_path

    gfh_ini = args.gfh_cfg_ini_path
    certs_ini = [args.key_ini_path, args.cnt_cfg_ini_path]

    bl_file = args.input_bl_path
    signed_bl_file = args.output_path

    code = check_file(gfh_ini)
    if code != CODE_SUCCESS:
        SLOG_E("GFH INI file is not found")
        return code

    if certs_ini is None or len(certs_ini) != 2:
        SLOG_E("Key/Content INI file are not set")

    code = check_file(certs_ini[0])
    if code != CODE_SUCCESS:
        SLOG_E("KEY INI file is not found")
        return code

    code = check_file(certs_ini[1])
    if code != CODE_SUCCESS:
        SLOG_E("CONTENT INI file is not found")
        return code

    code = check_folder(intermediate_dir)
    if code != CODE_SUCCESS:
        SLOG_E("Intermediate folder is not found")
        return code

    code = check_file(bl_file)
    if code != CODE_SUCCESS:
        SLOG_E("Bootloader file is not found")
        return code
    SLOG_I("====== Sign Bootloader ======")
    SLOG_I("OUT File: " + os.path.realpath(signed_bl_file))
    SLOG_I("Intermediate DIR: " + os.path.realpath(intermediate_dir))
    SLOG_I("BL File: " + os.path.realpath(bl_file))
    SLOG_I("                            ")

    code = bl.load(bl_file)
    if code != CODE_SUCCESS:
        SLOG_E("Load Preloader Image failure: " + hex(code))
        return code

    code = bl.load_ini(gfh_ini, certs_ini)
    if code != CODE_SUCCESS:
        SLOG_E("Load INI failure: " + hex(code))
        return code

    code = bl.sign(key_cert_path)
    if code != CODE_SUCCESS:
        SLOG_E("Sign failure: " + hex(code))
        return code

    code = bl.pack(signed_bl_file)
    if code != CODE_SUCCESS:
        SLOG_E("Pack failure: " + hex(code))
        return code

    signed_bl_file = os.path.realpath(signed_bl_file)
    SLOG_I("Encoded Signed BL Image successfully, please check content")
    SLOG_I("CHECK: " + signed_bl_file)
    return CODE_SUCCESS

def _op_sign_rma(args):
    """
    Sign/re-sign preloader operation
    """
    if args.intermediate_dir:
        intermediate_dir = args.intermediate_dir
    else:
        intermediate_dir = args.tmp_output_path

    code, rma = RMA.create("RMA", intermediate_dir)
    if code != CODE_SUCCESS:
        SLOG_E("Create RMA-CERT failure: " + hex(code))
        return code

    key_cert_path = args.key_cert_path
    primary_dbg_cert = args.primary_dbg_cert_path


    gfh_ini = args.gfh_cfg_ini_path
    certs_ini = [args.key_ini_path, args.primary_dbg_ini_path
                                  , args.secondary_dbg_ini_path]

    #bl_file = args.input_bl_path
    signed_rma_file = args.output_path

    code = check_file(gfh_ini)
    if code != CODE_SUCCESS:
        SLOG_E("GFH INI file is not found")
        return code

    if certs_ini is None or len(certs_ini) != 3:
        SLOG_E("Key/Primary/Secondary INI file are not set")

    code = check_file(certs_ini[0])
    if code != CODE_SUCCESS:
        SLOG_E("KEY INI file is not found")
        return code

    code = check_file(certs_ini[1])
    if code != CODE_SUCCESS:
        SLOG_E("PRIMARY INI file is not found")
        return code

    code = check_file(certs_ini[2])
    if code != CODE_SUCCESS:
        SLOG_E("SECONDARY INI file is not found")
        return code

    code = check_folder(intermediate_dir)
    if code != CODE_SUCCESS:
        SLOG_E("Intermediate folder is not found")
        return code

    SLOG_I("=========== Sign RMA ===========")
    SLOG_I("OUT File: " + os.path.realpath(signed_rma_file))
    SLOG_I("Intermediate DIR: " + os.path.realpath(intermediate_dir))
    SLOG_I("                            ")

    code = rma.load_ini(gfh_ini, certs_ini)
    if code != CODE_SUCCESS:
        SLOG_E("Load INI failure: " + hex(code))
        return code

    code = rma.sign(key_cert_path)
    if code != CODE_SUCCESS:
        SLOG_E("Sign failure: " + hex(code))
        return code

    code = rma.pack(signed_rma_file)
    if code != CODE_SUCCESS:
        SLOG_E("Pack failure: " + hex(code))
        return code

    signed_rma_file = os.path.realpath(signed_rma_file)
    SLOG_I("Encoded Signed RMA Image successfully, please check content")
    SLOG_I("CHECK: " + signed_rma_file)
    return CODE_SUCCESS

def _op_oem_key_header(args):

    key_path = args.key_path
    hsm = args.hsm
    str_sec_level = args.sec_level

    if key_path is None or key_path == '':
       SLOG_E("KEY_PATH is not found")
       return CODE_INVALID_INPUT

    is_hsm = False
    sec_level = 0
    if hsm is not None and hsm == '1':
        is_hsm = True

    if str_sec_level is not None and str_sec_level != '':
        sec_level = int(str_sec_level)

    if not is_hsm:
        code = check_file(key_path)
        if code != CODE_SUCCESS:
            SLOG_E("KEY_PATH is not valid")
            return code

    code = remove_file(args.output_path)
    if code != CODE_SUCCESS:
        SLOG_E("OUTPUT FILE PATH IS INVALID")
        return code

    flag2 = ''
    if sec_level == 0:
        flag2 = 'RSA2048'
    elif sec_level == 1:
        flag2 = 'RSA3072'
    elif sec_level == 2:
        flag2 = 'RSA4096'
    else:
        SLOG_E("SEC_LEVEL IS NOT SUPPORTED")
        return CODE_NOT_SUPPORT

    code, master = SignMaster.create()
    if code != CODE_SUCCESS:
        SLOG_E("CREATE SIGN_MASTER FAIURE")
        return code

    code, pubk = master.public_key(is_hsm, key_path, flag2)
    if code != CODE_SUCCESS:
        return code

    code = master.pubk_to_key_definition(flag2, pubk, "OEM", args.output_path)
    if code != CODE_SUCCESS:
        return code

    SLOG_I("Encoded public Key header successfully, please check content")
    SLOG_I("CHECK: " + os.path.realpath(args.output_path))
    return CODE_SUCCESS

def _op_oem_keyhash_bin(args):

    key_path = args.key_path
    hsm = args.hsm
    str_sec_level = args.sec_level

    if key_path is None or key_path == '':
       SLOG_E("KEY_PATH is not found")
       return CODE_INVALID_INPUT

    is_hsm = False
    sec_level = 0
    if hsm is not None and hsm == '1':
        is_hsm = True

    if str_sec_level is not None and str_sec_level != '':
        sec_level = int(str_sec_level)

    if not is_hsm:
        code = check_file(key_path)
        if code != CODE_SUCCESS:
            SLOG_E("KEY_PATH is not valid")
            return code

    code = remove_file(args.output_path)
    if code != CODE_SUCCESS:
        SLOG_E("OUTPUT FILE PATH IS INVALID")
        return code

    flag1 = ''
    flag2 = ''
    if sec_level == 0:
        flag2 = 'RSA2048'
        flag1 = 'RSA2048/SHA256'
    elif sec_level == 1:
        flag2 = 'RSA3072'
        flag1 = 'RSA3072/SHA384'
    elif sec_level == 2:
        flag2 = 'RSA4096'
        flag1 = 'RSA4096/SHA384'
    else:
        SLOG_E("SEC_LEVEL IS NOT SUPPORTED")
        return CODE_NOT_SUPPORT

    code, master = SignMaster.create()
    if code != CODE_SUCCESS:
        SLOG_E("CREATE SIGN_MASTER FAIURE")
        return code

    code, pubk = master.public_key(is_hsm, key_path, flag2)
    if code != CODE_SUCCESS:
        return code

    code = master.pubk_to_hash(flag1, pubk, args.output_path)
    if code != CODE_SUCCESS:
        return code

    SLOG_I("Encoded public Key hash successfully, please check content")
    SLOG_I("CHECK: " + os.path.realpath(args.output_path))
    return CODE_SUCCESS

def _op_oem_keyhash_ex_bin(args):

    key_path = args.key_path
    hsm = args.hsm
    str_sec_level = args.sec_level

    if key_path is None or key_path == '':
       SLOG_E("KEY_PATH is not found")
       return CODE_INVALID_INPUT

    is_hsm = False
    sec_level = 0
    if hsm is not None and hsm == '1':
        is_hsm = True

    if str_sec_level is not None and str_sec_level != '':
        sec_level = int(str_sec_level)

    if not is_hsm:
        code = check_file(key_path)
        if code != CODE_SUCCESS:
            SLOG_E("KEY_PATH is not valid")
            return code

    code = remove_file(args.output_path)
    if code != CODE_SUCCESS:
        SLOG_E("OUTPUT FILE PATH IS INVALID")
        return code

    flag1 = ''
    flag2 = ''
    if sec_level == 0:
        flag2 = 'RSA2048'
        flag1 = 'RSA2048/SHA256'
    elif sec_level == 1:
        flag2 = 'RSA3072'
        flag1 = 'RSA3072/SHA384'
    elif sec_level == 2:
        flag2 = 'RSA4096'
        flag1 = 'RSA4096/SHA384'
    else:
        SLOG_E("SEC_LEVEL IS NOT SUPPORTED")
        return CODE_NOT_SUPPORT

    code, master = SignMaster.create()
    if code != CODE_SUCCESS:
        SLOG_E("CREATE SIGN_MASTER FAIURE")
        return code

    code, pubk = master.public_key(is_hsm, key_path, flag2)
    if code != CODE_SUCCESS:
        return code

    code = master.pubk_to_hash(flag1, pubk, args.output_path)
    if code != CODE_SUCCESS:
        SLOG_E("COMPUTE KEYHASH FAILURE: " + hex(code))
        return code

    with open(args.output_path, 'rb') as src:
        while True:
            raw = src.read(4);
            if len(raw) != 4:
                break
            data = struct.unpack('<I', raw)

    SLOG_I("Encoded public Key hash successfully, please check content")
    SLOG_I("CHECK: " + os.path.realpath(args.output_path))
    return CODE_SUCCESS

def _op_da_key_header(args):

    key_path = args.key_path
    hsm = args.hsm
    str_sec_level = args.sec_level

    if key_path is None or key_path == '':
       SLOG_E("KEY_PATH is not found")
       return CODE_INVALID_INPUT

    is_hsm = False
    sec_level = 0
    if hsm is not None and hsm == '1':
        is_hsm = True

    if str_sec_level is not None and str_sec_level != '':
        sec_level = int(str_sec_level)

    if not is_hsm:
        code = check_file(key_path)
        if code != CODE_SUCCESS:
            SLOG_E("KEY_PATH is not valid")
            return code

    code = remove_file(args.output_path)
    if code != CODE_SUCCESS:
        SLOG_E("OUTPUT FILE PATH IS INVALID")
        return code

    flag2 = ''
    if sec_level == 0:
        flag2 = 'RSA2048'
    elif sec_level == 1:
        flag2 = 'RSA3072'
    elif sec_level == 2:
        flag2 = 'RSA4096'
    else:
        SLOG_E("SEC_LEVEL IS NOT SUPPORTED")
        return CODE_NOT_SUPPORT

    code, master = SignMaster.create()
    if code != CODE_SUCCESS:
        SLOG_E("CREATE SIGN_MASTER FAIURE")
        return code

    code, pubk = master.public_key(is_hsm, key_path, flag2)
    if code != CODE_SUCCESS:
        return code

    code = master.pubk_to_key_definition(flag2, pubk, "DA", args.output_path)
    if code != CODE_SUCCESS:
        return code

    SLOG_I("Encoded public Key header successfully, please check content")
    SLOG_I("CHECK: " + os.path.realpath(args.output_path))
    return CODE_SUCCESS

def _op_keybin(args):
    """
    Generate root key data structure for root public key authentication.
    """
    code, config = load_config(args.key_ini_path)
    if code != CODE_SUCCESS:
        SLOG_E("CONFIG_INI IS INVALID")
        return code

    if 'KEY' not in config:
        SLOG_E("FORMAT OF KEY_INI IS INVALID")
        return CODE_INVALID_INPUT

    key_config = config['KEY']
    if 'rootkey' not in key_config or 'imgkey' not in key_config:
        SLOG_E("FORMAT OF KEY_INI IS INVALID")
        return CODE_INVALID_INPUT

    code = remove_file(args.output_path)
    if code != CODE_SUCCESS:
        SLOG_E("OUTPUT FILE PATH IS INVALID")
        return code

    is_hsm = 0
    root_key = ''
    flag1 = ''
    flag2 = ''
    sec_level = 0
    if 'sec_level' in key_config:
        sec_level = int(key_config['sec_level'])
    if 'hsm' in key_config:
        if '1' == key_config['hsm']:
            is_hsm = 1

    root_key = key_config['rootkey']
    if sec_level == 0:
        flag2 = 'RSA2048'
    elif sec_level == 1:
        flag2 = 'RSA3072'
    elif sec_level == 2:
        flag2 = 'RSA4096'
    else:
        SLOG_E("SEC_LEVEL IS NOT SUPPORTED")
        return CODE_NOT_SUPPORT

    code, master = SignMaster.create()
    if code != CODE_SUCCESS:
        SLOG_E("CREATE SIGN_MASTER FAIURE")
        return code

    code, pubk = master.public_key(is_hsm, root_key, flag2)
    if code != CODE_SUCCESS:
        return code

    code = master.pubk_to_proprietary(flag2, pubk, args.output_path)
    if code != CODE_SUCCESS:
        return code

    SLOG_I("Encoded Public Key successfully, please check content")
    SLOG_I("CHECK: " + os.path.realpath(args.output_path))
    return CODE_SUCCESS

def _op_keybin_pss(args):
    """
    Root key data structures are different for different padding. Here we handles pss padding.
    """
    args.padding = 'pss'
    return _op_keybin(args)

def _op_keybin_legacy(args):
    """
    Root key data structures are different for different padding. Here we handles legacy padding.
    """
    args.padding = 'legacy'
    return _op_keybin(args)

def _op_keyhash(args):
    """
    Generate hash of root key data structure, which is dependent on padding used.
    """
    code, config = load_config(args.key_ini_path)
    if code != CODE_SUCCESS:
        SLOG_E("CONFIG_INI IS INVALID")
        return code

    if 'KEY' not in config:
        SLOG_E("FORMAT OF KEY_INI IS INVALID")
        return CODE_INVALID_INPUT

    key_config = config['KEY']
    if 'rootkey' not in key_config or 'imgkey' not in key_config:
        SLOG_E("FORMAT OF KEY_INI IS INVALID")
        return CODE_INVALID_INPUT

    code = remove_file(args.output_path)
    if code != CODE_SUCCESS:
        SLOG_E("OUTPUT FILE PATH IS INVALID")
        return code

    is_hsm = 0
    root_key = ''
    flag1 = ''
    flag2 = ''
    sec_level = 0
    if 'sec_level' in key_config:
        sec_level = int(key_config['sec_level'])
    if 'hsm' in key_config:
        if '1' == key_config['hsm']:
            is_hsm = 1

    root_key = key_config['rootkey']
    if sec_level == 0:
        flag1 = 'RSA2048/SHA256'
        flag2 = 'RSA2048'
    elif sec_level == 1:
        flag1 = 'RSA3072/SHA384'
        flag2 = 'RSA3072'
    elif sec_level == 2:
        flag1 = 'RSA4096/SHA384'
        flag2 = 'RSA4096'
    else:
        SLOG_E("SEC_LEVEL IS NOT SUPPORTED")
        return CODE_NOT_SUPPORT

    code, master = SignMaster.create()
    if code != CODE_SUCCESS:
        SLOG_E("CREATE SIGN_MASTER FAIURE")
        return code

    code, pubk = master.public_key(is_hsm, root_key, flag2)
    if code != CODE_SUCCESS:
        return code

    code = master.pubk_to_hash(flag1, pubk, args.output_path, False)
    if code != CODE_SUCCESS:
        return code

    SLOG_I("Encoded Public Key Hash successfully, please check content")
    SLOG_I("CHECK: " + os.path.realpath(args.output_path))
    return CODE_SUCCESS

def _op_keyhash_pss(args):
    """
    Root key data struture hash for pss padding.
    """
    code, config = load_config(args.key_ini_path)
    if code != CODE_SUCCESS:
        SLOG_E("CONFIG_INI IS INVALID")
        return code

    if 'KEY' not in config:
        SLOG_E("FORMAT OF KEY_INI IS INVALID")
        return CODE_INVALID_INPUT

    key_config = config['KEY']
    if 'rootkey' not in key_config or 'imgkey' not in key_config:
        SLOG_E("FORMAT OF KEY_INI IS INVALID")
        return CODE_INVALID_INPUT

    code = remove_file(args.output_path)
    if code != CODE_SUCCESS:
        SLOG_E("OUTPUT FILE PATH IS INVALID")
        return code

    is_hsm = 0
    root_key = ''
    flag1 = ''
    flag2 = ''
    sec_level = 0
    if 'sec_level' in key_config:
        sec_level = int(key_config['sec_level'])
    if 'hsm' in key_config:
        if '1' == key_config['hsm']:
            is_hsm = 1

    root_key = key_config['rootkey']
    if sec_level == 0:
        flag1 = 'RSA2048/SHA256'
        flag2 = 'RSA2048'
    elif sec_level == 1:
        flag1 = 'RSA3072/SHA384'
        flag2 = 'RSA3072'
    elif sec_level == 2:
        flag1 = 'RSA4096/SHA384'
        flag2 = 'RSA4096'
    else:
        SLOG_E("SEC_LEVEL IS NOT SUPPORTED")
        return CODE_NOT_SUPPORT

    code, master = SignMaster.create()
    if code != CODE_SUCCESS:
        SLOG_E("CREATE SIGN_MASTER FAIURE")
        return code

    code, pubk = master.public_key(is_hsm, root_key, flag2)
    if code != CODE_SUCCESS:
        return code

    code = master.pubk_to_hash(flag1, pubk, args.output_path)
    if code != CODE_SUCCESS:
        return code

    SLOG_I("Encoded Public Key Hash successfully, please check content")
    SLOG_I("CHECK: " + os.path.realpath(args.output_path))
    return CODE_SUCCESS

def _op_keyhash_declaration(args):
    """
    Root key data struture hash for pss padding.
    """
    code, config = load_config(args.key_ini_path)
    if code != CODE_SUCCESS:
        SLOG_E("CONFIG_INI IS INVALID")
        return code

    if 'KEY' not in config:
        SLOG_E("FORMAT OF KEY_INI IS INVALID")
        return CODE_INVALID_INPUT

    key_config = config['KEY']
    if 'rootkey' not in key_config or 'imgkey' not in key_config:
        SLOG_E("FORMAT OF KEY_INI IS INVALID")
        return CODE_INVALID_INPUT

    code = remove_file(args.output_path)
    if code != CODE_SUCCESS:
        SLOG_E("OUTPUT FILE PATH IS INVALID")
        return code

    if args.intermediate_dir:
        intermediate_dir = args.intermediate_dir
    else:
        intermediate_dir = args.tmp_output_path

    key_hash = os.path.join(intermediate_dir, "KEY_HASH.bin")
    code = remove_file(key_hash)
    if code != CODE_SUCCESS:
        SLOG_E("OUTPUT KEY_HASH TEMP IS INVALID")
        return code

    is_hsm = 0
    root_key = ''
    flag1 = ''
    flag2 = ''
    sec_level = 0
    if 'sec_level' in key_config:
        sec_level = int(key_config['sec_level'])
    if 'hsm' in key_config:
        if '1' == key_config['hsm']:
            is_hsm = 1

    root_key = key_config['rootkey']
    if sec_level == 0:
        flag1 = 'RSA2048/SHA256'
        flag2 = 'RSA2048'
    elif sec_level == 1:
        flag1 = 'RSA3072/SHA384'
        flag2 = 'RSA3072'
    elif sec_level == 2:
        flag1 = 'RSA4096/SHA384'
        flag2 = 'RSA4096'
    else:
        SLOG_E("SEC_LEVEL IS NOT SUPPORTED")
        return CODE_NOT_SUPPORT

    code, master = SignMaster.create()
    if code != CODE_SUCCESS:
        SLOG_E("CREATE SIGN_MASTER FAIURE")
        return code

    code, pubk = master.public_key(is_hsm, root_key, flag2)
    if code != CODE_SUCCESS:
        return code

    code = master.pubk_to_hash(flag1, pubk, key_hash)
    if code != CODE_SUCCESS:
        return code

    idx = 0
    size = os.path.getsize(key_hash)
    with open(args.output_path, 'wb') as dist:
        dist.write('    unsigned int keyhash[] = {\n')
        with open(key_hash, 'rb') as src:
            while size > 0:
                raw = src.read(4)
                value = struct.unpack("<I", raw)
                if idx == 0:
                    dist.write('        ')
                dist.write(hex(value[0]))
                size -= 4
                if size > 0:
                    dist.write(", ")
                idx += 1
                if idx == 4:
                    dist.write("\n")
                    idx = 0
        dist.write('    };')

    remove_file(key_hash)
    SLOG_I("Encoded Public Key Hash successfully, please check content")
    SLOG_I("CHECK: " + os.path.realpath(args.output_path))
    return CODE_SUCCESS

def _op_keyhash_legacy(args):
    """
    Root key data struture hash for legacy padding.
    """
    args.padding = 'legacy'
    return _op_keyhash(args)

def pbp_op(args):
    """
    Handles and dispatches all operations supported by pbp.
    """
    supported_ops = {
        'sign'                  : _op_sign_bl,
        'sign_bl'               : _op_sign_bl,
        'toolauth'              : _op_sign_toolauth,
        'rma'                   : _op_sign_rma,
        'oem_key_export'        : _op_oem_key_header,
        'oem_keyhash_export'    : _op_oem_keyhash_bin,
        'oem_keyhash_ex_export' : _op_oem_keyhash_ex_bin,
        'da_key_export'         : _op_da_key_header,
        'keybin_pss'            : _op_keybin_pss,
        'keybin_legacy'         : _op_keybin_legacy,
        'keyhash_brom'          : _op_keyhash_declaration,
        'keyhash_pss'           : _op_keyhash_pss,
        'keyhash_legacy'        : _op_keyhash_legacy
    }

    if args.output_path is None:
        print ("output path is not given!")
        return -1

    if args.op is None:
        print ("op is not given!")
        return -1

    if args.op == 'sign':
        if not args.input_bl_path:
            print ("bootloader path is not given!")
            return -1
        if (args.key_ini_path is None) and (args.key_cert_path is None):
            print ("key path is not given!")
            return -1
    else:
        if (args.key_ini_path is None) and (args.key_path is None):
            print ("key path is not given!")
            return -1

    args.tmp_output_path = os.path.dirname(os.path.abspath(args.output_path))
    if not os.path.exists(args.tmp_output_path):
        os.makedirs(args.tmp_output_path)

    op_f = supported_ops.get(args.op)
    if op_f is None:
        SLOG_E("UNSUPPORTED FUNCTION: " + args.op)
        return -1

    return op_f(args)


def main():




    """
    Main function for pbp, which is used when pbp.py is executed directly.
    Note that we changed input bootloader parameter to -in_bl $BL_PATH.
    Please remember to add -in_bl if you're migrating from previous version.
    """
    DESC = 'PBPv2 tool for PL/TOOLAUTH/RMA-CERT generation'
    parser = argparse.ArgumentParser(description=DESC)
    parser.add_argument('-c', dest='cnt_cfg_ini_path',
                        help='content certificate configuration path')
    parser.add_argument('-i', dest='key_ini_path',
                        help='key configuartion path')
    parser.add_argument('-q', dest='primary_dbg_ini_path',
                        help='primary dbg configuartion path')
    parser.add_argument('-s', dest='secondary_dbg_ini_path',
                        help='secondary dbg configuartion path')
    parser.add_argument('-g', dest='gfh_cfg_ini_path',
                        help='gfh(generaic file header) configuration path')
    parser.add_argument('-k', dest='key_cert_path',
                        help='key certificate path')
    parser.add_argument('-p', dest='primary_dbg_cert_path',
                        help='primary dbg certificate path')
    parser.add_argument('-o', dest='output_path',
                        help='output file path')
    parser.add_argument('-m', dest='intermediate_dir',
                        help='intermediate files folder')
    parser.add_argument('-hsm', dest='hsm',
                        help='Use hsm')
    parser.add_argument('-key_path', dest='key_path',
                        help='Key path or key label')
    parser.add_argument('-size', dest='packet_size',
                        help='key hash bin size')
    parser.add_argument('-sec_level', dest='sec_level',
                        help='Sec_level')
    parser.add_argument('-func', dest='op',
                        help='operation to be performed', required=True)
    parser.add_argument('input_bl_path', nargs='?',
                        help='input file path')

    pbp_args = parser.parse_args()
    try:
        result = pbp_op(pbp_args)
        if result != 0:
            print("[PBP] Sign Error: code = %s" %(str(result)))
            sys.exit(1)
    except:
        print("===============================================================")
        traceback.print_exc()
        print("===============================================================")
        sys.exit(1)
    return result


if __name__ == '__main__':
    main()

