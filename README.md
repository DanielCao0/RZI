# RZI

![Zephyr](https://img.shields.io/badge/Zephyr-4.4.99-blue)
![Board](https://img.shields.io/badge/board-RAK4631-green)
![Stack](https://img.shields.io/badge/stack-LoRa%20Basics%20Modem-orange)

**RZI** is a LoRaWAN Class A node firmware for the [RAK4631](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview/) (nRF52840 + SX1262), built on [Zephyr RTOS](https://github.com/zephyrproject-rtos/zephyr) and Semtech's [LoRa Basics Modem](https://github.com/Lora-net/usp) via the [usp_zephyr](https://github.com/Lora-net/usp_zephyr) module.

The application performs an OTAA join, then transmits a 4-byte counter on port 1 every 60 seconds. Logs are available over the USB CDC console (115200 8N1); the blue LED indicates network join, the green LED toggles on each transmission.

## Features

- OTAA activation with DevEUI / JoinEUI / AppKey defined in Devicetree
- Semtech USP / LoRa Basics Modem stack (not Zephyr's `CONFIG_LORAWAN`)
- West T2 topology: `app/` is the manifest repository, all dependency revisions pinned in [`app/west.yml`](app/west.yml)
- Local fixes to `usp_zephyr` managed with the official `west patch` mechanism ([`app/zephyr/patches.yml`](app/zephyr/patches.yml))
- Fully containerized build (Dockerfile + wrapper script), no local toolchain needed
- J-Link SWD flashing script

## Repository Layout

| Path | Description |
|------|-------------|
| `app/` | Firmware sources and west manifest |
| `app/boards/rak4631_nrf52840.overlay` | Board overlay: SX1262 wiring, LoRaWAN credentials and region |
| `app/zephyr/` | `west patch` files for `usp_zephyr` |
| `doc/` | Design notes (Chinese): west topologies, patch workflow, USP/LBM layers |
| `zephyr-docker.sh` | Containerized build entry point |
| `flash-rak4631.sh` | J-Link SWD flashing helper |
| `Dockerfile.zephyr` | Build image (Zephyr SDK + west) |

Third-party source trees (`zephyr/`, `modules/`, `usp_zephyr/`, ...) are fetched by `west update` and are not part of this repository.

## Getting Started

### Prerequisites

- Docker
- J-Link debugger (SWD) for flashing

### Build and Flash

```bash
git clone git@github.com:DanielCao0/RZI.git
cd RZI

./zephyr-docker.sh build-image   # build the Docker image (once)
./zephyr-docker.sh init          # west init -l app + west update (once)
./zephyr-docker.sh build         # build app/ for rak4631/nrf52840
./flash-rak4631.sh               # flash app/build/zephyr/zephyr.hex
```

## Configuration

LoRaWAN credentials and region live in the `zephyr,user` node of [`app/boards/rak4631_nrf52840.overlay`](app/boards/rak4631_nrf52840.overlay):

| Property | Description |
|----------|-------------|
| `user-lorawan-device-eui` | DevEUI |
| `user-lorawan-join-eui` | JoinEUI (all-zero by default on ChirpStack) |
| `user-lorawan-app-key` | OTAA AppKey (LoRaWAN 1.0; LBM `set_nwkkey`) |
| `user-lorawan-gen_app-key` | Gen App Key (LoRaWAN 1.1 AppKey; may match AppKey on 1.0 networks) |
| `user-lorawan-region` | `EU_868`, `US_915`, `CN_470`, `AS_923_GRP1`, ... |

Rebuild and reflash after changing these values.

## Build Script Reference

```text
./zephyr-docker.sh build-image   Build the Dockerfile.zephyr image
./zephyr-docker.sh init          Initialize the west workspace and apply patches
./zephyr-docker.sh build         Build app/ (default: rak4631/nrf52840)
./zephyr-docker.sh sample        Build the official usp_zephyr periodical_uplink
                                 sample (nRF52840 DK + SX126x shield, not RAK4631)
./zephyr-docker.sh patch         west patch clean + apply
./zephyr-docker.sh patch-list    Show patch status
./zephyr-docker.sh shell         Open a shell inside the container
./zephyr-docker.sh clangd        Export compile_commands.json for host clangd
```

## Documentation

Design notes (in Chinese) live in [`doc/`](./doc/README.md):

- [Docker environment, step by step](./doc/zephyr-docker-environment-explained.md)
- [West topologies T1 / T2 / T3](./doc/west-topology.md)
- [west patch workflow](./doc/west-patch.md)
- [USP / LBM / Zephyr: how the pieces fit](./doc/usp-lbm-zephyr.md)
- [RAK4631 board Devicetree walkthrough](./doc/rak4631-dts.md)

## Dependencies

| Component | Source | Revision |
|-----------|--------|----------|
| Zephyr | [zephyrproject-rtos/zephyr](https://github.com/zephyrproject-rtos/zephyr) | 4.4.99 (pinned commit `161f758`) |
| usp_zephyr | [Lora-net/usp_zephyr](https://github.com/Lora-net/usp_zephyr) | `main` |
| usp (LBM + RAC) | [Lora-net/usp](https://github.com/Lora-net/usp) | `main` (with submodules) |
