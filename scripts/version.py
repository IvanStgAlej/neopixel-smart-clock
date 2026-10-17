# PlatformIO pre-script: embeds the firmware version as the FW_VERSION macro.
# Version comes from `git describe --tags --always --dirty`:
#   - tag present  -> as-is (v1.0.0, v1.0.0-3-gabc123, ...-dirty)
#   - no tags      -> 0.0.0-dev+<hash>
#   - git failure  -> 0.0.0-dev
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)


def get_version():
    try:
        out = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=env.subst("$PROJECT_DIR"),  # noqa: F821
            stderr=subprocess.DEVNULL,
        ).decode().strip()
    except Exception:
        return "0.0.0-dev"
    if not out:
        return "0.0.0-dev"
    # Without tags, describe --always returns only a short hash (maybe + "-dirty").
    if out[0] in "0123456789abcdef" and not out.startswith("v") and "-g" not in out:
        return "0.0.0-dev+" + out
    return out


version = get_version()
print("Firmware version: " + version)
env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(version))])  # noqa: F821
