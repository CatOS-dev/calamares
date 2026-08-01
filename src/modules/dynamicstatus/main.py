# SPDX-License-Identifier: CC0-1.0
import libcalamares

status = "Install base system"


def pretty_name():
    return "Install base system"


def pretty_status_message():
    return status


def run():
    global status
    status = "Downloading packages: 128 MiB / 1.00 GiB - 12.5 MiB/s"
    libcalamares.job.setprogress(0.125)
    return None
