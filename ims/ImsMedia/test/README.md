# Manual ImsMedia test applications

This directory holds optional platform test applications. Automated regression
suites and the host/device validation matrix are documented in
[../tests/README.md](../tests/README.md).

| Target | Source | Purpose |
| --- | --- | --- |
| `ImsMediaTestingApp` | `app/ImsMediaTestingApp/app` | Manual media-session controls and test UI |
| `TestImsMediaHal` | `imsmediahal` | Test implementation of the radio IMS media AIDL interface |

Build in a complete Android product checkout:

```sh
m ImsMediaTestingApp TestImsMediaHal
```

Both are privileged, platform-signed apps. The HAL test app also shares the phone
UID. Install them through a dedicated test product using that product's signing,
privileged-permission and SELinux configuration. They are not part of normal
`imsmedia.mk` product integration.

The test HAL exposes an Android bound service. Its manifest and Java service do
not by themselves register a vendor `IImsMedia/default` instance. Editing a VINTF
manifest alone cannot provide that service. Configure the intended test binding
or real HAL integration in the product before exercising that path.

The paired ImsStack/ImsMedia flow uses `ImsMediaService`. Use these applications
for focused media-session or HAL-interface checks; complete product integration
also requires carrier provisioning, IMS registration, signing and device audio
route validation.
