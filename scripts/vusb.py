# Builds the V-USB driver from the upstream git submodule. V-USB has no usable PlatformIO package,
# and the repository is not a PlatformIO library (it also contains host tools), so only usbdrv/ is compiled.
# Runs twice: before the build to fetch V-USB and add its headers, after the build environment is
# complete (it has the MCU flags then) to compile the driver.
import subprocess
from os.path import join

Import("env")

usbdrv = join(env.subst("$PROJECT_DIR"), "external", "v-usb", "usbdrv")

if "PIOMAINPROG" not in env:
    # "-": not initialized, "+": another commit checked out, e.g. after switching branches without updating submodules.
    status = subprocess.run(["git", "submodule", "status", "external/v-usb"], cwd=env.subst("$PROJECT_DIR"), capture_output=True, text=True).stdout
    if status.startswith(("-", "+")):
        subprocess.check_call(["git", "submodule", "update", "--init", "external/v-usb"], cwd=env.subst("$PROJECT_DIR"))
    env.Append(CPPPATH=[usbdrv])
else:
    vusb_env = env.Clone()
    vusb_env.Append(CPPPATH=["$PROJECT_INCLUDE_DIR"],  # usbconfig.h
                    CCFLAGS=["-Wno-overflow"])         # usbdrv.c stores 0xA0 in a char array, which is fine but warns
    env.Append(LIBS=[vusb_env.BuildLibrary(join("$BUILD_DIR", "vusb"), usbdrv, "-<*> +<usbdrv.c> +<usbdrvasm.S> +<oddebug.c>")])
