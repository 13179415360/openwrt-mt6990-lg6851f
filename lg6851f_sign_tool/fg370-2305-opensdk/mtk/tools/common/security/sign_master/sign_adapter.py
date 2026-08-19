"""
This module is a adapter of real cryptographic engine, such like OpenSSL(SW),
or HSM (HW). This module also provide a proprietary way to manage keys in file
repositories.
"""

__copyright__ = "Copyright 2017-2022, MediaTek Inc."
__version__ = "3.0.1"

import os
import re
import subprocess
import uuid

from lib.sign_error import CODE_SUCCESS
from lib.sign_error import CODE_INVALID_INPUT
from lib.sign_error import CODE_KEY_FILE_NOT_FOUND
from lib.sign_error import CODE_EXECUTE_ENGINE_FAIL

from lib.sign_util import SLOG_E
from lib.sign_util import load_config
from lib.sign_util import check_folder
from lib.sign_util import remove_file

def __run_program(cmd, isCD = False):
    try:
        if isCD:
            new_cmd = "cd " + self.runable_dir + ";" + cmd
        else:
            new_cmd = cmd
        subprocess.check_call(new_cmd, shell = True)
        return CODE_SUCCESS
    except subprocess.CalledProcessError as e:
        SLOG_E(new_cmd + " code = " + str(e.returncode))
        return CODE_EXECUTE_ENGINE_FAIL

def __openssl_rsa_sign(key_file, hashfile, sigfile, hashalog, salt, pad):
    ins = "openssl pkeyutl -sign -in " + hashfile + " "
    ins += "-inkey " + key_file + " "
    ins += "-out " + sigfile + " "
    ins += "-pkeyopt digest:" + hashalog + " "

    if pad == 'pss':
        ins += "-pkeyopt rsa_padding_mode:pss "
        ins += "-pkeyopt rsa_pss_saltlen:" + str(salt)
    else:
        ins += "-raw "
    return __run_program(ins)

def __openssl_ec_sign(key_file, hashfile, sigfile, hashalog):
    ins = "openssl pkeyutl -sign -inkey " + key_file + " -in "
    ins += hashfile + " > " + sigfile

    return __run_program(ins)

def __hsm_key_dir():
    """Private Function: Find Software HSM Keys

    Returns:
        folder: Root Folder of hsm_test_keys
    """
    base_dir = os.path.dirname(__file__)
    platform_dir = None
    if 'MTK_PLATFORM' in os.environ:
        platform = os.environ['MTK_PLATFORM']
        platform = platform.upper()
        if platform != None and 'MT' in platform:
            platform_dir = os.path.join(base_dir, 'platform/' + platform)

    root_dir = ''
    config = None
    config_file = os.path.join(base_dir, 'sign_adapter.ini')
    if platform_dir is not None:
        platform_config_file = os.path.join(platform_dir, 'sign_adapter.ini')
        code, config = load_config(platform_config_file)
        if code != CODE_SUCCESS or 'GLOBAL' not in config or \
            'ROOT_DIR' not in config['GLOBAL'] or \
            'TEMP_DIR' not in config['GLOBAL']:
            config = None

    if config is None:
        code, config = load_config(config_file)
        if code != CODE_SUCCESS:
            config = None

    if config is None or 'GLOBAL' not in config or \
       'ROOT_DIR' not in config['GLOBAL'] or \
       'TEMP_DIR' not in config['GLOBAL']:
        root_dir = os.path.join(base_dir, "hsm_test_keys")
        intermediate_dir = os.path.join(base_dir, "intermediate")
        return root_dir, intermediate_dir

    root_dir = config['GLOBAL']['ROOT_DIR']
    intermediate_dir = config['GLOBAL']['TEMP_DIR']

    root_dir = os.path.realpath(os.path.join(base_dir, root_dir))
    intermediate_dir = os.path.realpath(os.path.join(base_dir, intermediate_dir))

    return root_dir, intermediate_dir

def sign(name, flag, msgdig):
    """Signing the message digest by asymmetric key such like RSA

    We can query public from security module like HSM by name.

    Args:
        name (string): Key identifier in security module
        flag (string): Key algorithm such like RSA2048/SHA384/PSS
        msgdig (byte string): Bytes string of message digest

    Returns:
        code: The return error message. 0 for success
        signature: The signature data
    """
    if flag is None or flag == '':
        return CODE_NOT_SUPPORT, None

    flag = flag.lower()
    flags = flag.split("/")
    if len(flags) < 2:
        return CODE_INVALID_INPUT, None

    # Decode parameter of signing such like 'RSA2048/SHA384/PSS'
    asynalog = flags[0].strip()
    asynbits = int(re.sub("[^0-9]", "", asynalog))
    hashalog = flags[1].strip()
    hashalog_bits = int(re.sub("[^0-9]", "", hashalog))
    # Python 3 '/' produces a float (for example 32.0), while OpenSSL's
    # rsa_pss_saltlen option requires an integer string.
    salt_size = hashalog_bits // 8

    if asynalog != 'rsa2048' and \
       asynalog != 'rsa3072' and \
       asynalog != 'rsa4096' and \
       asynalog != 'ec384':
       return CODE_NOT_SUPPORT, None

    if hashalog != 'sha256' and \
       hashalog != 'sha384':
       return CODE_NOT_SUPPORT, None

    pad = ''
    if "rsa" in asynalog:
        if len(flags) != 3:
            return CODE_INVALID_INPUT, None
        pad = flags[2]
    elif "ec" in asynalog and len(flags) != 2:
        return CODE_INVALID_INPUT, None

    # Generate path of private key
    keys_root_folder, intermediate_dir = __hsm_key_dir()
    if not os.path.isdir(keys_root_folder):
        os.makedirs(keys_root_folder)
    keys_folder = os.path.join(keys_root_folder, asynalog.upper())
    if not os.path.isdir(keys_folder):
        os.makedirs(keys_folder)

    key_name = name + "_prvk.pem"
    key_file = os.path.join(keys_folder, key_name)
    if not os.path.isfile(key_file):
        SLOG_E("KEY FILE NOT FOUND: " + key_file)
        return CODE_KEY_FILE_NOT_FOUND, None

    # Create intermediate folders
    code = check_folder(intermediate_dir)
    if code!= CODE_SUCCESS:
        SLOG_E("Intermediate folder cannot be created: " + intermediate_dir)
        return code, None

    uid = str(uuid.uuid4())
    hashfile = os.path.join(intermediate_dir, "DIGEST_" + uid + ".bin")
    sigfile = os.path.join(intermediate_dir, "SIG_" + uid + ".bin")

    # Dump Hashdata to file
    with open(hashfile, 'wb') as fout:
        fout.write(msgdig)

    if "rsa" in asynalog:
        code = __openssl_rsa_sign(key_file, hashfile, sigfile,
                                hashalog, salt_size, pad)
    else:
        code = __openssl_ec_sign(key_file, hashfile, sigfile, hashalog)

    if code != CODE_SUCCESS:
        return code, None

    raw = b''
    with open(sigfile, 'rb') as sigin:
        raw = sigin.read()
        if "rsa" in asynalog:
            if len(raw) != (asynbits // 8):
                return CODE_EXECUTE_ENGINE_FAIL, None

    return CODE_SUCCESS, raw

def _public_key(name, flag):
    """Retrive the public key binary path by key name

    This API won't be called by SignMaster until it's name rename to public_key
    We can query public from security module like HSM by name.
    The public key MUST decode as DER(ASN.1)

    Args:
        name (string): Key identifier in security module
        flag (string): Key algorithm such like RSA2048

    Returns:
        code: The return error message. 0 for success
        raw: The encoded key binary

    """

    # TO DO: NEED TO RETURN PUBLIC KEY DATA WITH DER FORMAT

def public_key_file(name, flag):
    """Retrive the public key file path by key name

    We can query public from security module like HSM by name.
    The public key MUST stored in files and as format of PKCS#8.
    The format of PKCS#8 can be DER(ASN.1) or PEM(DER with base64 encoding)

    Args:
        name (string): Key identifier in security module
        flag (string): Key algorithm such like RSA2048

    Returns:
        code: The return error message. 0 for success
        filenme: The filename of public key

    """
    if flag is None or flag == '':
        return CODE_NOT_SUPPORT, None

    # Decode parameter such like 'RSA2048'
    asynalog = flag.strip().lower()
    asynbits = int(re.sub("[^0-9]", "", asynalog))

    if asynalog != 'rsa2048' and \
       asynalog != 'rsa3072' and \
       asynalog != 'rsa4096' and \
       asynalog != 'ec384':
       return CODE_NOT_SUPPORT, None

    # Generate path of private key
    keys_root_folder, folder = __hsm_key_dir()
    if not os.path.isdir(keys_root_folder):
        os.makedirs(keys_root_folder)
    keys_folder = os.path.join(keys_root_folder, asynalog.upper())
    if not os.path.isdir(keys_folder):
        os.makedirs(keys_folder)

    key_name = name + "_prvk.pem"
    key_file = os.path.join(keys_folder, key_name)
    if not os.path.isfile(key_file):
        SLOG_E("KEY FILE NOT FOUND: " + key_file)
        return CODE_KEY_FILE_NOT_FOUND, None

    return CODE_SUCCESS, key_file
