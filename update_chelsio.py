import glob
import os
import subprocess


# This script updates chelsio-related binaries and scripts we ship with TrueNAS
# Download the latest "Chelsio Unified Wire vx.y.z for Linux" from https://service.chelsio.com/ and place it here

BASE = "tools/chelsio_adapter_config_v4"


def update(src, arch):
    bin_suffix = "_arm" if arch == "arm64" else ""
    os.makedirs(f"files/{arch}/usr/sbin", exist_ok=True)
    os.makedirs("files/usr/local/libexec", exist_ok=True)
    os.makedirs("files/usr/lib/firmware/cxgb4/config", exist_ok=True)

    subprocess.check_call(
        f"cp {src}/{BASE}/bin/new/chelsio_adapter_config.py files/usr/local/libexec/",
        shell=True,
    )
    subprocess.check_call(
        f"cp {src}/{BASE}/vpds/* files/usr/lib/firmware/cxgb4/config/",
        shell=True,
    )
    subprocess.check_call(
        f"mv {src}/{BASE}/bin/t5seeprom_bins/t5seeprom{bin_suffix} files/{arch}/usr/sbin/t5seeprom",
        shell=True,
    )
    subprocess.check_call(
        f"mv {src}/{BASE}/bin/t6seeprom_bins/t6seeprom{bin_suffix} files/{arch}/usr/sbin/t6seeprom",
        shell=True,
    )


if __name__ == "__main__":
    source = glob.glob("ChelsioUwire-*.tar.gz")[0]
    src = source.removesuffix(".tar.gz")
    subprocess.check_call(["tar", "xf", source])
    try:
        update(src, "amd64")
        update(src, "arm64")
    finally:
        subprocess.check_call(["rm", "-rf", src])
