#!/usr/bin/env python3
import argparse
import json
import sys
import time

import dbus


BLUEZ_SERVICE = "org.bluez"
ADAPTER_IFACE = "org.bluez.Adapter1"
DEVICE_IFACE = "org.bluez.Device1"
GATT_CHAR_IFACE = "org.bluez.GattCharacteristic1"
PROPERTIES_IFACE = "org.freedesktop.DBus.Properties"
OBJECT_MANAGER_IFACE = "org.freedesktop.DBus.ObjectManager"

DEFAULT_DEVICE_NAME = "RLCD-Calender"
SERVICE_UUID = "8d2f3a70-7c5a-4db8-9e42-8f2d9b5b9a01"
CHARACTERISTIC_UUID = "8d2f3a71-7c5a-4db8-9e42-8f2d9b5b9a01"


def get_managed_objects(bus):
    manager = dbus.Interface(bus.get_object(BLUEZ_SERVICE, "/"), OBJECT_MANAGER_IFACE)
    return manager.GetManagedObjects()


def find_adapter(bus, adapter_name):
    objects = get_managed_objects(bus)
    for path, interfaces in objects.items():
        if ADAPTER_IFACE in interfaces and path.endswith("/" + adapter_name):
            return path
    for path, interfaces in objects.items():
        if ADAPTER_IFACE in interfaces:
            return path
    return None


def set_adapter_powered(bus, adapter_path):
    props = dbus.Interface(bus.get_object(BLUEZ_SERVICE, adapter_path), PROPERTIES_IFACE)
    props.Set(ADAPTER_IFACE, "Powered", dbus.Boolean(1))


def device_matches(props, name):
    candidate_names = [
        str(props.get("Name", "")),
        str(props.get("Alias", "")),
    ]
    uuids = [str(uuid).lower() for uuid in props.get("UUIDs", [])]
    return name in candidate_names or SERVICE_UUID.lower() in uuids


def find_connected_device(bus, adapter_path, name, address):
    objects = get_managed_objects(bus)
    normalized_address = address.upper() if address else ""
    for path, interfaces in objects.items():
        props = interfaces.get(DEVICE_IFACE)
        if not props or not path.startswith(adapter_path + "/"):
            continue
        if not bool(props.get("Connected", False)):
            continue
        if normalized_address and str(props.get("Address", "")).upper() != normalized_address:
            continue
        if normalized_address or device_matches(props, name):
            return path, props
    return None, None


def services_resolved(bus, device_path):
    props = dbus.Interface(bus.get_object(BLUEZ_SERVICE, device_path), PROPERTIES_IFACE)
    try:
        return bool(props.Get(DEVICE_IFACE, "ServicesResolved"))
    except dbus.exceptions.DBusException:
        return False


def find_characteristic(bus, device_path):
    objects = get_managed_objects(bus)
    for path, interfaces in objects.items():
        props = interfaces.get(GATT_CHAR_IFACE)
        if not props or not path.startswith(device_path + "/"):
            continue
        if str(props.get("UUID", "")).lower() == CHARACTERISTIC_UUID:
            return path
    return None


def write_payload(bus, char_path, payload):
    characteristic = dbus.Interface(bus.get_object(BLUEZ_SERVICE, char_path), GATT_CHAR_IFACE)
    characteristic.WriteValue(
        dbus.Array([dbus.Byte(byte) for byte in payload], signature="y"),
        dbus.Dictionary({"type": dbus.String("request")}, signature="sv"),
    )


def main():
    parser = argparse.ArgumentParser(description="Send RLCD notification through BLE GATT.")
    parser.add_argument("--title", default="测试通知", help="popup title")
    parser.add_argument("--content", required=True, help="popup content")
    parser.add_argument("--seconds", type=int, default=8, help="display seconds, 1-60")
    parser.add_argument("--device-name", default=DEFAULT_DEVICE_NAME)
    parser.add_argument("--address", default="", help="optional connected BLE device address")
    parser.add_argument("--adapter", default="hci0")
    args = parser.parse_args()

    seconds = max(1, min(60, args.seconds))
    payload = json.dumps(
        {"t": args.title, "c": args.content, "s": seconds},
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")
    print(f"payload={payload.decode('utf-8')} bytes={len(payload)}", flush=True)

    bus = dbus.SystemBus()
    adapter_path = find_adapter(bus, args.adapter)
    if not adapter_path:
        print("No BLE adapter found. Is bluetoothd running?", file=sys.stderr, flush=True)
        return 1
    set_adapter_powered(bus, adapter_path)

    device_path, device_props = find_connected_device(bus, adapter_path, args.device_name, args.address)
    if not device_path:
        print(
            "Device is not currently connected. Connect it first, then rerun this script.",
            file=sys.stderr,
            flush=True,
        )
        return 2

    print(
        f"using connected device path={device_path} name={device_props.get('Name', '')} "
        f"alias={device_props.get('Alias', '')} addr={device_props.get('Address', '')}",
        flush=True,
    )

    if not services_resolved(bus, device_path):
        print("Connected device exists, but GATT services are not resolved.", file=sys.stderr, flush=True)
        return 3

    char_path = find_characteristic(bus, device_path)
    if not char_path:
        print(f"Characteristic not found: {CHARACTERISTIC_UUID}", file=sys.stderr, flush=True)
        return 4

    print(f"writing characteristic {char_path}", flush=True)
    write_payload(bus, char_path, payload)
    print("done", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
