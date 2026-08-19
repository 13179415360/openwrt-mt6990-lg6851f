ARCH:=aarch64
SUBTARGET:=mt6990
BOARDNAME:=MT6990 / T830
CPU_TYPE:=cortex-a55
CPU_SUBTYPE:=neon-vfpv4
KERNELNAME:=Image dtbs

FEATURES+=boot-part ext4 fpu gpio pci pcie rootfs-part rtc squashfs usb

# LG6851F uses an F2FS loop overlay.  On a newly created/rootfs-replaced
# overlay, mount_root invokes mkfs.f2fs before it can persist configuration.
# Keep both creation and repair tools in every MT6990 image.
DEFAULT_PACKAGES += fitblk kmod-usb3 libf2fs mkf2fs f2fsck
