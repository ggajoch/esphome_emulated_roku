# Emulated Roku ESPHome Component

An ESPHome external component that emulates a Roku device, allowing ESP32 devices to be discovered and controlled by Roku-compatible remotes and hubs (such as Logitech Harmony).

## Features

- **SSDP Discovery**: Responds to SSDP M-SEARCH requests for `roku:ecp` and `ssdp:all`
- **Roku ECP API**: Implements the Roku External Control Protocol (ECP) HTTP API
- **Key Press Events**: All key presses are passed to YAML via `on_key_press` trigger
- **Harmony Hub Compatible**: Works with Logitech Harmony Hub and other Roku-compatible controllers
- **Reliable Multicast**: Uses raw BSD sockets with periodic IGMP membership refresh for reliable SSDP discovery

## Installation

Add the component to your ESPHome configuration:

```yaml
external_components:
  - source:
      type: local
      path: components
```

## Configuration

```yaml
emulated_roku:
  device_name: "My Emulated Roku"  # Optional, default: "ESPHome Roku"
  port: 8060                        # Optional, default: 8060
  on_key_press:                     # Optional, triggered on every key event
    - lambda: |-
        ESP_LOGI("roku", "Key: %s -> %s", type.c_str(), key.c_str());
```

### Configuration Variables

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `device_name` | string | `ESPHome Roku` | The friendly name shown during discovery |
| `port` | int | `8060` | HTTP port for the Roku ECP API |
| `on_key_press` | automation | - | Triggered when a key event is received |

### on_key_press Trigger

The `on_key_press` trigger provides two parameters:

| Parameter | Type | Description |
|-----------|------|-------------|
| `type` | `std::string` | Event type: `keypress`, `keydown`, or `keyup` |
| `key` | `std::string` | The key name (see Key Names below) |
