SUMMARY = "A free implementation of the OpenGL API"
DESCRIPTION = "Mesa is an open-source implementation of the OpenGL specification - \
a system for rendering interactive 3D graphics.  \
A variety of device drivers allows Mesa to be used in many different environments \
ranging from software emulation to complete hardware acceleration for modern GPUs. \
Mesa is used as part of the overall Direct Rendering Infrastructure and X.org \
environment."

HOMEPAGE = "http://mesa3d.org"
BUGTRACKER = "https://bugs.freedesktop.org"
SECTION = "x11"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://docs/license.rst;md5=17a4ea65de7a9ab42437f3131e616a7f"

PE = "2"

SRC_URI = "https://mesa.freedesktop.org/archive/mesa-${PV}.tar.xz \
           file://0001-meson.build-check-for-all-linux-host_os-combinations.patch \
           file://0002-meson.build-make-TLS-ELF-optional.patch \
           file://0001-meson-misdetects-64bit-atomics-on-mips-clang.patch \
           file://0001-futex.h-Define-__NR_futex-if-it-does-not-exist.patch \
           file://0001-v3d-vc4-Fix-dmabuf-import-for-non-scanout-buffers.patch \
           "

SRC_URI[sha256sum] = "022c7293074aeeced2278c872db4fa693147c70f8595b076cf3f1ef81520766d"

UPSTREAM_CHECK_GITTAGREGEX = "mesa-(?P<pver>\d+(\.\d+)+)"

#because we cannot rely on the fact that all apps will use pkgconfig,
#make eglplatform.h independent of MESA_EGL_NO_X11_HEADER
do_install:append() {
    if ${@bb.utils.contains('PACKAGECONFIG', 'egl', 'true', 'false', d)}; then
        sed -i -e 's/^#elif defined(__unix__) && defined(EGL_NO_X11)$/#elif defined(__unix__) \&\& defined(EGL_NO_X11) || ${@bb.utils.contains('PACKAGECONFIG', 'x11', '0', '1', d)}/' ${D}${includedir}/EGL/eglplatform.h
    fi
}

DEPENDS = "expat makedepend-native flex-native bison-native libxml2-native zlib chrpath-replacement-native python3-mako-native gettext-native"
EXTRANATIVEPATH += "chrpath-native"
PROVIDES = " \
    ${@bb.utils.contains('PACKAGECONFIG', 'opengl', 'virtual/libgl', '', d)} \
    ${@bb.utils.contains('PACKAGECONFIG', 'gles', 'virtual/libgles1 virtual/libgles2', '', d)} \
    ${@bb.utils.contains('PACKAGECONFIG', 'egl', 'virtual/egl', '', d)} \
    ${@bb.utils.contains('PACKAGECONFIG', 'gbm', 'virtual/libgbm', '', d)} \
    virtual/mesa \
    "

inherit meson pkgconfig python3native gettext features_check

BBCLASSEXTEND = "native nativesdk"

ANY_OF_DISTRO_FEATURES:class-target = "opengl vulkan"

PLATFORMS ??= "${@bb.utils.filter('PACKAGECONFIG', 'x11 wayland', d)}"

export YOCTO_ALTERNATE_EXE_PATH = "${STAGING_LIBDIR}/llvm${MESA_LLVM_RELEASE}/llvm-config"
export YOCTO_ALTERNATE_MULTILIB_NAME = "${base_libdir}"
export LLVM_CONFIG = "${STAGING_BINDIR_NATIVE}/llvm-config${MESA_LLVM_RELEASE}"
export WANT_LLVM_RELEASE = "${MESA_LLVM_RELEASE}"

MESA_LLVM_RELEASE ?= "${LLVMVERSION}"

# set the MESA_BUILD_TYPE to either 'release' (default) or 'debug'
# by default the upstream mesa sources build a debug release
# here we assume the user will want a release build by default
MESA_BUILD_TYPE ?= "release"
def check_buildtype(d):
    _buildtype = d.getVar('MESA_BUILD_TYPE')
    if _buildtype not in ['release', 'debug']:
        bb.fatal("unknown build type (%s), please set MESA_BUILD_TYPE to either 'release' or 'debug'" % _buildtype)
    if _buildtype == 'debug':
        return 'debugoptimized'
    return 'plain'
MESON_BUILDTYPE = "${@check_buildtype(d)}"

EXTRA_OEMESON = " \
    -Dshared-glapi=enabled \
    -Dgallium-opencl=disabled \
    -Dglx-read-only-text=true \
    -Dplatforms='${@",".join("${PLATFORMS}".split())}' \
"

def strip_comma(s):
    return s.strip(',')

PACKAGECONFIG:class-target ??= "${@bb.utils.filter('DISTRO_FEATURES', 'wayland vulkan', d)} \
                   ${@bb.utils.contains('DISTRO_FEATURES', 'opengl', 'opengl egl gles gbm dri gallium virgl', '', d)} \
                   ${@bb.utils.contains('DISTRO_FEATURES', 'x11 opengl', 'x11 dri3', '', d)} \
                   ${@bb.utils.contains('DISTRO_FEATURES', 'x11 vulkan', 'dri3', '', d)} \
                   elf-tls \
		   "
PACKAGECONFIG:class-native ?= "gbm gallium egl opengl elf-tls x11"
PACKAGECONFIG:class-nativesdk ?= "gbm gallium egl opengl elf-tls x11"

PACKAGECONFIG:remove:libc-musl = "elf-tls"

# "gbm" requires "dri", "opengl"
PACKAGECONFIG[gbm] = "-Dgbm=enabled,-Dgbm=disabled"

X11_DEPS = "xorgproto virtual/libx11 libxext libxxf86vm libxdamage libxfixes xrandr"
# "x11" requires "opengl"
PACKAGECONFIG[x11] = ",-Dglx=disabled,${X11_DEPS}"
PACKAGECONFIG[elf-tls] = "-Delf-tls=true, -Delf-tls=false"
PACKAGECONFIG[xvmc] = "-Dgallium-xvmc=enabled,-Dgallium-xvmc=disabled,libxvmc"
PACKAGECONFIG[wayland] = ",,wayland-native wayland libdrm wayland-protocols"

# "dri" requires "opengl"
PACKAGECONFIG[dri] = "-Ddri-drivers=${@strip_comma('${DRIDRIVERS}')}, -Ddri-drivers='', xorgproto libdrm"
PACKAGECONFIG[dri3] = "-Ddri3=enabled, -Ddri3=disabled, xorgproto libxshmfence"

# Vulkan drivers need dri3 enabled
# amd could be enabled as well but requires gallium-llvm with llvm >= 3.9
VULKAN_DRIVERS = ""
VULKAN_DRIVERS:append:x86:class-target = ",intel"
VULKAN_DRIVERS:append:x86-64:class-target = ",intel"
VULKAN_DRIVERS:append ="${@bb.utils.contains('PACKAGECONFIG', 'freedreno', ',freedreno', '', d)}"
PACKAGECONFIG[vulkan] = "-Dvulkan-drivers=${@strip_comma('${VULKAN_DRIVERS}')}, -Dvulkan-drivers='',"

PACKAGECONFIG[opengl] = "-Dopengl=true, -Dopengl=false"

# "gles" requires "opengl"
PACKAGECONFIG[gles] = "-Dgles1=enabled -Dgles2=enabled, -Dgles1=disabled -Dgles2=disabled"

# "egl" requires "dri", "opengl"
PACKAGECONFIG[egl] = "-Degl=enabled, -Degl=disabled"

PACKAGECONFIG[etnaviv] = ""
PACKAGECONFIG[freedreno] = ""
PACKAGECONFIG[kmsro] = ""
PACKAGECONFIG[vc4] = ""
PACKAGECONFIG[v3d] = ""

GALLIUMDRIVERS = "swrast"
# gallium swrast was found to crash Xorg on startup in x32 qemu
GALLIUMDRIVERS:x86-x32 = ""
# Add crocus when 21.2 is out to the list below to support the full range of Intel GPUs
GALLIUMDRIVERS:append:x86:class-target = ",i915,iris"
GALLIUMDRIVERS:append:x86-64:class-target = ",i915,iris"

GALLIUMDRIVERS:append ="${@bb.utils.contains('PACKAGECONFIG', 'etnaviv', ',etnaviv', '', d)}"
GALLIUMDRIVERS:append ="${@bb.utils.contains('PACKAGECONFIG', 'freedreno', ',freedreno', '', d)}"
GALLIUMDRIVERS:append ="${@bb.utils.contains('PACKAGECONFIG', 'kmsro', ',kmsro', '', d)}"
GALLIUMDRIVERS:append ="${@bb.utils.contains('PACKAGECONFIG', 'vc4', ',vc4', '', d)}"
GALLIUMDRIVERS:append ="${@bb.utils.contains('PACKAGECONFIG', 'v3d', ',v3d', '', d)}"

# radeonsi requires LLVM
GALLIUMDRIVERS_RADEONSI = "${@bb.utils.contains('PACKAGECONFIG', 'r600', ',radeonsi', '', d)}"
GALLIUMDRIVERS_LLVM = "r300,svga,nouveau${GALLIUMDRIVERS_RADEONSI}"

PACKAGECONFIG[r600] = ""
PACKAGECONFIG[virgl] = ""

GALLIUMDRIVERS:append = "${@bb.utils.contains('PACKAGECONFIG', 'gallium-llvm', ',${GALLIUMDRIVERS_LLVM}', '', d)}"
GALLIUMDRIVERS:append = "${@bb.utils.contains('PACKAGECONFIG', 'r600', ',r600', '', d)}"
GALLIUMDRIVERS:append = "${@bb.utils.contains('PACKAGECONFIG', 'virgl', ',virgl', '', d)}"

PACKAGECONFIG[gallium] = "-Dgallium-drivers=${@strip_comma('${GALLIUMDRIVERS}')}, -Dgallium-drivers='', libdrm"
PACKAGECONFIG[gallium-llvm] = "-Dllvm=enabled -Dshared-llvm=enabled, -Dllvm=disabled, llvm${MESA_LLVM_RELEASE} llvm-native \
                               elfutils"
PACKAGECONFIG[xa]  = "-Dgallium-xa=enabled, -Dgallium-xa=disabled"
PACKAGECONFIG[va] = "-Dgallium-va=enabled,-Dgallium-va=disabled,libva-initial"

PACKAGECONFIG[vdpau] = "-Dgallium-vdpau=enabled,-Dgallium-vdpau=disabled,libvdpau"

PACKAGECONFIG[lima] = ""
GALLIUMDRIVERS:append ="${@bb.utils.contains('PACKAGECONFIG', 'lima', ',lima', '', d)}"

PACKAGECONFIG[panfrost] = ""
GALLIUMDRIVERS:append ="${@bb.utils.contains('PACKAGECONFIG', 'panfrost', ',panfrost', '', d)}"

PACKAGECONFIG[osmesa] = "-Dosmesa=true,-Dosmesa=false"

PACKAGECONFIG[unwind] = "-Dlibunwind=enabled,-Dlibunwind=disabled,libunwind"

PACKAGECONFIG[lmsensors] = "-Dlmsensors=enabled,-Dlmsensors=disabled,lmsensors"

# llvmpipe is slow if compiled with -fomit-frame-pointer (e.g. -O2)
FULL_OPTIMIZATION:append = " -fno-omit-frame-pointer"

CFLAGS:append:armv5 = " -DMISSING_64BIT_ATOMICS"
CFLAGS:append:armv6 = " -DMISSING_64BIT_ATOMICS"

# Remove the mesa dependency on mesa-dev, as mesa is empty
RDEPENDS:${PN}-dev = ""

# Add dependency so that GLES3 header don't need to be added manually
RDEPENDS:libgles2-mesa-dev += "libgles3-mesa-dev"

PACKAGES =+ "libegl-mesa libegl-mesa-dev \
             libosmesa libosmesa-dev \
             libgl-mesa libgl-mesa-dev \
             libglapi libglapi-dev \
             libgbm libgbm-dev \
             libgles1-mesa libgles1-mesa-dev \
             libgles2-mesa libgles2-mesa-dev \
             libgles3-mesa libgles3-mesa-dev \
             libxatracker libxatracker-dev \
             mesa-megadriver mesa-vulkan-drivers \
             mesa-vdpau-drivers \
            "

do_install:append () {
    # Drivers never need libtool .la files
    rm -f ${D}${libdir}/dri/*.la
    rm -f ${D}${libdir}/egl/*.la
    rm -f ${D}${libdir}/gallium-pipe/*.la
    rm -f ${D}${libdir}/gbm/*.la

    # it was packaged in libdricore9.1.3-1 and preventing upgrades when debian.bbclass was used 
    chrpath --delete ${D}${libdir}/dri/*_dri.so || true

    # libwayland-egl has been moved to wayland 1.15+
    rm -f ${D}${libdir}/libwayland-egl*
    rm -f ${D}${libdir}/pkgconfig/wayland-egl.pc
}

# For the packages that make up the OpenGL interfaces, inject variables so that
# they don't get Debian-renamed (which would remove the -mesa suffix), and
# RPROVIDEs/RCONFLICTs on the generic libgl name.
python __anonymous() {
    pkgconfig = (d.getVar('PACKAGECONFIG') or "").split()
    suffix = ""
    if "-native" in d.getVar("PN"):
        suffix = "-native"
    for p in (("egl", "libegl", "libegl1"),
              ("dri", "libgl", "libgl1"),
              ("gles", "libgles1", "libglesv1-cm1"),
              ("gles", "libgles2", "libglesv2-2"),
              ("gles", "libgles3",)):
        if not p[0] in pkgconfig:
            continue
        mlprefix = d.getVar("MLPREFIX")
        fullp = mlprefix + p[1] + "-mesa" + suffix
        mlprefix = d.getVar("MLPREFIX")
        pkgs = " " + " ".join(mlprefix + x + suffix for x in p[1:])
        d.setVar("DEBIAN_NOAUTONAME:" + fullp, "1")
        d.appendVar("RREPLACES:" + fullp, pkgs)
        d.appendVar("RPROVIDES:" + fullp, pkgs)
        d.appendVar("RCONFLICTS:" + fullp, pkgs)

        d.appendVar("RRECOMMENDS:" + fullp, " ${MLPREFIX}mesa-megadriver" + suffix)

        # For -dev, the first element is both the Debian and original name
        fullp = mlprefix + p[1] + "-mesa-dev" + suffix
        pkgs = " " + mlprefix + p[1] + "-dev" + suffix
        d.setVar("DEBIAN_NOAUTONAME:" + fullp, "1")
        d.appendVar("RREPLACES:" + fullp, pkgs)
        d.appendVar("RPROVIDES:" + fullp, pkgs)
        d.appendVar("RCONFLICTS:" + fullp, pkgs)
}

python mesa_populate_packages() {
    pkgs = ['mesa', 'mesa-dev', 'mesa-dbg']
    for pkg in pkgs:
        d.setVar("RPROVIDES:%s" % pkg, pkg.replace("mesa", "mesa-dri", 1))
        d.setVar("RCONFLICTS:%s" % pkg, pkg.replace("mesa", "mesa-dri", 1))
        d.setVar("RREPLACES:%s" % pkg, pkg.replace("mesa", "mesa-dri", 1))

    import re
    dri_drivers_root = oe.path.join(d.getVar('PKGD'), d.getVar('libdir'), "dri")
    if os.path.isdir(dri_drivers_root):
        dri_pkgs = os.listdir(dri_drivers_root)
        lib_name = d.expand("${MLPREFIX}mesa-megadriver")
        for p in dri_pkgs:
            m = re.match(r'^(.*)_dri\.so$', p)
            if m:
                pkg_name = " ${MLPREFIX}mesa-driver-%s" % legitimize_package_name(m.group(1))
                d.appendVar("RPROVIDES:%s" % lib_name, pkg_name)
                d.appendVar("RCONFLICTS:%s" % lib_name, pkg_name)
                d.appendVar("RREPLACES:%s" % lib_name, pkg_name)

    pipe_drivers_root = os.path.join(d.getVar('libdir'), "gallium-pipe")
    do_split_packages(d, pipe_drivers_root, r'^pipe_(.*)\.so$', 'mesa-driver-pipe-%s', 'Mesa %s pipe driver', extra_depends='')
}

PACKAGESPLITFUNCS:prepend = "mesa_populate_packages "

PACKAGES_DYNAMIC += "^mesa-driver-.*"
PACKAGES_DYNAMIC:class-native = "^mesa-driver-.*-native"

FILES:mesa-megadriver = "${libdir}/dri/* ${datadir}/drirc.d/00-mesa-defaults.conf"
FILES:mesa-vulkan-drivers = "${libdir}/libvulkan_*.so ${datadir}/vulkan"
FILES:${PN}-vdpau-drivers = "${libdir}/vdpau/*.so.*"
FILES:libegl-mesa = "${libdir}/libEGL.so.*"
FILES:libgbm = "${libdir}/libgbm.so.*"
FILES:libgles1-mesa = "${libdir}/libGLESv1*.so.*"
FILES:libgles2-mesa = "${libdir}/libGLESv2.so.*"
FILES:libgl-mesa = "${libdir}/libGL.so.*"
FILES:libglapi = "${libdir}/libglapi.so.*"
FILES:libosmesa = "${libdir}/libOSMesa.so.*"
FILES:libxatracker = "${libdir}/libxatracker.so.*"

FILES:${PN}-dev = "${libdir}/pkgconfig/dri.pc ${includedir}/vulkan ${libdir}/vdpau/*.so"
FILES:libegl-mesa-dev = "${libdir}/libEGL.* ${includedir}/EGL ${includedir}/KHR ${libdir}/pkgconfig/egl.pc"
FILES:libgbm-dev = "${libdir}/libgbm.* ${libdir}/pkgconfig/gbm.pc ${includedir}/gbm.h"
FILES:libgl-mesa-dev = "${libdir}/libGL.* ${includedir}/GL ${libdir}/pkgconfig/gl.pc"
FILES:libglapi-dev = "${libdir}/libglapi.*"
FILES:libgles1-mesa-dev = "${libdir}/libGLESv1*.* ${includedir}/GLES ${libdir}/pkgconfig/glesv1*.pc"
FILES:libgles2-mesa-dev = "${libdir}/libGLESv2.* ${includedir}/GLES2 ${libdir}/pkgconfig/glesv2.pc"
FILES:libgles3-mesa-dev = "${includedir}/GLES3"
FILES:libosmesa-dev = "${libdir}/libOSMesa.* ${includedir}/GL/osmesa.h ${libdir}/pkgconfig/osmesa.pc"
FILES:libxatracker-dev = "${libdir}/libxatracker.so ${libdir}/libxatracker.la \
                          ${includedir}/xa_tracker.h ${includedir}/xa_composite.h ${includedir}/xa_context.h \
                          ${libdir}/pkgconfig/xatracker.pc"

# Fix upgrade path from mesa to mesa-megadriver
RREPLACES:mesa-megadriver = "mesa"
RCONFLICTS:mesa-megadriver = "mesa"
RPROVIDES:mesa-megadriver = "mesa"
