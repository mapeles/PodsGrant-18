# PodsGrant for iOS 18

This fork updates [PodsGrant](https://github.com/LNSSPsd/PodsGrant) for newer AirPods on iOS 18. It maps unsupported AirPods product identifiers to compatible models in `bluetoothd` and the system pairing flow.

## Compatibility

This release was developed and tested on the following environment:

- iPhone XS
- iOS 18.7.1 (22H31)
- Dopamine 3.0.9, rootless
- ElleKit 1.2

Other devices, iOS versions, jailbreaks, and `bluetoothd` builds are not currently tested and may not work. To avoid unsafe function hooks, the tweak verifies the exact iOS 18.7.1 `bluetoothd` build before installing runtime hooks and disables those hooks when verification fails.

이 릴리스는 iPhone XS, iOS 18.7.1, Dopamine 3.0.9 환경에 맞추어 개발 및 테스트되었습니다. 다른 기기, iOS 빌드 또는 탈옥 환경에서는 작동하지 않을 수 있습니다.

## Installation

1. Download the rootless `.deb` from the latest GitHub release.
2. Install it with Sileo or another compatible package manager.
3. Restart SpringBoard from Settings > PodsGrant.
4. If the AirPods were previously paired with a broken model name, forget the device and pair it again.

## Building

Install [Theos](https://theos.dev), then run:

```sh
make clean package FINALPACKAGE=1 THEOS=/path/to/theos
```

The package is intentionally rootless and arm64e-only.

## Credits

- [Ruphane / LNSSPsd](https://github.com/LNSSPsd) for the original PodsGrant project
- [mapeles](https://github.com/mapeles/PodsGrant-18) for the iOS 18 port
- [Torrekie](https://twitter.com/torrekie) for the icon

Licensed under the MIT License. See [LICENSE](LICENSE).
