import glob
import os
import subprocess


# This script updates chelsio-related binaries and scripts we ship with TrueNAS
# Download the latest "Chelsio Unified Wire vx.y.z for Linux" from https://service.chelsio.com/ and place it here

BASE = "tools/chelsio_adapter_config_v4"


if __name__ == "__main__":
    source = glob.glob("ChelsioUwire-*.tar.gz")[0]
    directory = source.removesuffix(".tar.gz")
    subprocess.check_call(["tar", "xf", source])
    try:
        os.makedirs("files/usr/sbin", exist_ok=True)
        os.makedirs("files/usr/lib/firmware/cxgb4/config", exist_ok=True)
        subprocess.check_call(
            f"mv {directory}/{BASE}/bin/new/chelsio_adapter_config.py files/usr/local/libexec",
            shell=True,
        )
        subprocess.check_call(
            f"mv {directory}/{BASE}/vpds/* files/usr/lib/firmware/cxgb4/config/",
            shell=True,
        )
        subprocess.check_call(
            f"mv {directory}/{BASE}/bin/t5seeprom_bins/t5seeprom files/usr/sbin/",
            shell=True,
        )
        subprocess.check_call(
            f"mv {directory}/{BASE}/bin/t6seeprom_bins/t6seeprom files/usr/sbin/",
            shell=True,
        )
    finally:
        subprocess.check_call(["rm", "-rf", directory])
