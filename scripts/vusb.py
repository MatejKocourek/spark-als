# Builds the V-USB driver from upstream, pinned to VUSB_COMMIT. V-USB has no usable PlatformIO package,
# and the repository is not a PlatformIO library (it also contains host tools), so only usbdrv/ is compiled.
# Runs twice: before the build to fetch V-USB and add its headers, after the build environment is
# complete (it has the MCU flags then) to compile the driver.
import io
import os
import shutil
import subprocess
import tarfile
from os.path import isfile, join, samefile
from urllib.request import urlopen

Import("env")

VUSB_COMMIT = "825abb82dd41936f384da74c54b1d462babda02c"  # Must match the external/v-usb submodule.

project = env.subst("$PROJECT_DIR")
vusb = join(project, "external", "v-usb")
usbdrv = join(vusb, "usbdrv")


def git(*args):
    return subprocess.run(["git", *args], cwd=project, capture_output=True, text=True)


def is_git_checkout():
    if not shutil.which("git"):
        return False
    top = git("rev-parse", "--show-toplevel")
    return top.returncode == 0 and samefile(top.stdout.strip(), project)


def download():
    # Without git, e.g. in a downloaded ZIP of this project, which does not contain submodules.
    url = "https://github.com/obdev/v-usb/archive/%s.tar.gz" % VUSB_COMMIT
    print("Downloading V-USB from " + url)
    with urlopen(url, timeout=60) as response:
        archive = tarfile.open(fileobj=io.BytesIO(response.read()), mode="r:gz")
    tmp = vusb + ".tmp"
    shutil.rmtree(tmp, ignore_errors=True)
    archive.extractall(tmp, **({"filter": "data"} if hasattr(tarfile, "data_filter") else {}))
    if os.path.isdir(vusb):
        os.rmdir(vusb)  # The empty submodule directory, fails if it is not empty.
    os.replace(join(tmp, "v-usb-" + VUSB_COMMIT), vusb)
    shutil.rmtree(tmp)


if "PIOMAINPROG" not in env:
    if is_git_checkout():
        recorded = git("ls-files", "--stage", "external/v-usb").stdout.split()
        if recorded and recorded[1] != VUSB_COMMIT:
            print("Error: external/v-usb is at %s, update VUSB_COMMIT in scripts/vusb.py" % recorded[1])
            env.Exit(1)
        # "-": not initialized, "+": another commit checked out, e.g. after switching branches without updating submodules.
        if git("submodule", "status", "external/v-usb").stdout.startswith(("-", "+")):
            subprocess.check_call(["git", "submodule", "update", "--init", "external/v-usb"], cwd=project)
    elif not isfile(join(usbdrv, "usbdrv.c")):
        download()
    env.Append(CPPPATH=[usbdrv])
else:
    vusb_env = env.Clone()
    vusb_env.Append(CPPPATH=["$PROJECT_INCLUDE_DIR"],  # usbconfig.h
                    CCFLAGS=["-Wno-overflow"])         # usbdrv.c stores 0xA0 in a char array, which is fine but warns
    env.Append(LIBS=[vusb_env.BuildLibrary(join("$BUILD_DIR", "vusb"), usbdrv, "-<*> +<usbdrv.c> +<usbdrvasm.S> +<oddebug.c>")])
