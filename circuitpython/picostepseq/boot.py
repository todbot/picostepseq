
import usb_hid
usb_hid.disable()
print("disabling USB HID")

import storage
storage.remount("/", readonly=False, disable_concurrent_write_protection=True)
print("making CIRCUITPY writable, w/ disable_concurrent_write_protection")

print("boot.py done")
