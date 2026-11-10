# LoRa Internet Gateway
## Technical Specification — Draft for Review

**Document status:** Draft — design requirements, not an implementation record  
**Revision:** 0.1  
**Date:** 10 November 2026  
**Related project:** Buoy  
**Initial gateway hardware:** Heltec WiFi LoRa 32 V3

---

## 1. Purpose and scope

The gateway provides an Internet bridge for a low-bandwidth LoRa mesh network. It receives telemetry from remote sensor nodes, buffers messages locally, and forwards them to the project's HTTPS backend.

The intended applications include:

- Distributed sensor networks transmitting small periodic telemetry packets.
- Remote telemetry and control applications, excluding the ROV project.
- Operation through intermittent Internet connectivity.
- Persistent local buffering while the backend is unavailable.
- Remote firmware maintenance.
- Future expansion to multiple gateways and sensor nodes.

The gateway will use an existing mesh protocol rather than implement a proprietary mesh-routing protocol. Mesh-specific functionality should be isolated behind an adapter so that the telemetry and backend layers are not unnecessarily coupled to one protocol.

### 1.1 Design principles

1. Reliability and data integrity take priority over convenience.
2. LoRa reception must continue during Internet outages.
3. Messages must not be discarded merely because the Internet is unavailable.
4. Network and firmware failures should be recoverable without physical intervention wherever practicable.
5. Configuration should be maintainable without recompiling firmware.
6. The architecture should allow future hardware and network expansion.
7. Local diagnostics should be available without connecting a computer.
8. Requirements and implementation status must be distinguished: this document describes planned behaviour, not implemented or validated functionality.

## 2. Hardware platform

### 2.1 Primary controller

The initial development platform is the **Heltec WiFi LoRa 32 V3**, comprising:

- ESP32-S3 microcontroller.
- Semtech SX1262 LoRa transceiver.
- 2.4 GHz Wi-Fi.
- Integrated OLED display.
- USB-C connectivity.
- On-board flash storage.

The exact board revision, flash capacity, PSRAM availability, pin allocation and peripheral conflicts must be verified against the physical board before the firmware partition table and hardware interfaces are finalised. Do not assume all nominal flash capacity is available to the application.

### 2.2 External RGB status indicator

The enclosure shall provide for an externally mounted, addressable RGB LED.

Proposed implementation:

- WS2812B/NeoPixel-compatible LED.
- Diffused, enclosure-mounted indicator, preferably replaceable.
- Three-wire connection: supply, data and ground.
- Appropriate 3.3 V-to-5 V data-level conversion where required by the selected LED.
- Series data resistor and local supply decoupling.
- Locking connector on the PCB.
- LED current included in the power budget.

The on-board LED remains available for basic diagnostic indication.

Proposed external LED states:

| Indication | Meaning |
|---|---|
| Blue, slow pulse | Initialisation |
| Cyan, double flash | Wi-Fi provisioning |
| Green, solid | Gateway operational |
| Green, brief flash | Successful HTTPS upload |
| Yellow, slow pulse | Internet unavailable; messages buffering |
| Purple, brief flash | LoRa packet received |
| Red, flashing | Critical fault |

Patterns, priorities and brightness limits are to be finalised during implementation. Packet reception must not make the indicator visually distracting.

### 2.3 Ethernet provision

The enclosure and PCB design should provide for optional wired Ethernet.

The Heltec board does not provide native Ethernet. An external Ethernet interface is therefore required. An SPI Ethernet controller such as the W5500 is an initial candidate, not a confirmed selection.

Before selection, verify:

- ESP32 driver and network-stack compatibility.
- TCP/IP integration with the chosen firmware framework.
- TLS and HTTPS support through the selected network interface.
- SPI resource and pin requirements.
- Power consumption and thermal behaviour.
- RJ45 MagJack, isolation, ESD and EMC provisions.
- Link and activity indication.

Ethernet is a planned expansion and is not a dependency for the first prototype.

### 2.4 Storage provision

The initial implementation should use internal flash for the persistent telemetry queue.

The flash partition layout must reserve sufficient capacity for:

- Bootloader and system partitions.
- Two application-image slots for OTA updates.
- Configuration storage.
- Persistent telemetry queue.
- Filesystem and diagnostic data.

An initial target of approximately 1 MiB for the telemetry queue is proposed, subject to confirmation of the physical flash capacity, OTA image sizes and partition layout. It is a design target, not a guaranteed available allocation.

LittleFS or an equivalent suitable filesystem should be evaluated for the persistent queue. NVS should hold configuration and small persistent state, not be used as a high-frequency telemetry log.

The queue implementation must address flash wear, interrupted writes, recovery after power loss, queue exhaustion and safe deletion of acknowledged records.

The PCB/enclosure design should retain the option of adding an SD-card interface in a future revision. An SD card is not required for the first prototype.

## 3. Mesh network

### 3.1 Protocol selection

The gateway shall use an existing mesh protocol.

**MeshCore** is the preferred initial candidate for evaluation. **Meshtastic** remains an alternative. This is a trial preference, not a final protocol selection.

Selection depends on successful testing of:

- Heltec WiFi LoRa 32 V3 compatibility.
- Firmware availability and maintenance.
- Companion/gateway interface support.
- Access to received telemetry and datagrams.
- Integration options for the gateway application.
- Radio configuration and UK regulatory compliance.
- Reliability and measured range.

The selected firmware's actual capabilities must be verified. Do not assume a standard companion image already provides the required HTTPS gateway service; custom firmware integration or a maintained fork may be necessary.

### 3.2 Mesh adapter

A defined mesh adapter interface shall separate protocol-specific functionality from the rest of the gateway.

The adapter should expose, as applicable:

- Received packet payload.
- Source node and network identifiers.
- Sequence/message identifier, where available.
- Radio RSSI and SNR.
- Receive timestamp or indication that it is unavailable.
- Packet type and decoding status.
- Transmit capability if required by the selected application.

The adapter must not make assumptions that all mesh protocols provide identical acknowledgements, routing information or metadata.

### 3.3 Radio operation and UK compliance

The final radio configuration must account for applicable UK 868 MHz requirements, including:

- Permitted frequency sub-band.
- Transmit power and antenna gain constraints.
- Duty-cycle or other access restrictions.
- Airtime limitations.
- Channel access requirements where applicable.

The exact operating frequency, bandwidth, spreading factor, coding rate, transmit power and antenna configuration remain open decisions until the protocol and regulatory sub-band are selected.

Range testing must consider antenna efficiency, antenna height, polarisation, terrain, obstructions and Fresnel-zone clearance. Lower data rates and higher spreading factors may improve link budget but increase airtime and network congestion.

## 4. Network connectivity

### 4.1 Wi-Fi

The gateway shall support 2.4 GHz Wi-Fi using the integrated ESP32-S3 radio.

- DHCP shall be the default IPv4 configuration.
- Static IPv4 configuration may be provided as an optional setting.
- The gateway shall retain configured credentials across normal reboot and power loss.
- Loss of Wi-Fi must not stop LoRa packet reception or local buffering.

### 4.2 Captive portal provisioning

A captive portal shall support initial Wi-Fi setup and subsequent network changes.

Required behaviour:

1. On first boot without usable credentials, start a temporary Wi-Fi access point.
2. Provide a configuration page accessible from a phone, tablet or computer.
3. Allow scanning for available Wi-Fi networks.
4. Allow the user to select an SSID and enter its password.
5. Test the new credentials before committing them.
6. Preserve existing working credentials if replacement credentials fail.
7. Store accepted configuration persistently.
8. Provide a physical-button method of entering provisioning mode.
9. Keep factory reset separate from ordinary network provisioning.
10. Provide clear feedback when provisioning succeeds or fails.

The setup access point should use a unique SSID (for example, `LoRa-Gateway-XXXX`) and a unique setup password. Credentials must not be written to serial logs or diagnostic uploads.

Captive-portal DNS redirection and common phone/desktop captive-network detection should be evaluated. A local configuration page should remain accessible even if automatic captive-portal detection does not work. An OLED QR code linking to the local page may be considered.

The portal should allow changes to the backend endpoint and gateway identity as well as Wi-Fi settings where appropriate. It must not require firmware reinstallation or factory reset to change networks.

### 4.3 Ethernet and network failover

When Ethernet is implemented:

- Prefer Ethernet when link and Internet connectivity are usable.
- Fall back to configured Wi-Fi if Ethernet is unavailable or unusable.
- Do not treat physical Ethernet link alone as proof of Internet access.
- If neither interface has Internet access, continue mesh reception and queue messages.
- Network changes must not corrupt the queue or interrupt radio processing.

DHCP shall be the default Ethernet configuration; static IPv4 may be supported as an option.

The network manager should distinguish link state, IP configuration, DNS availability and backend reachability.

## 5. Time synchronisation

The gateway shall obtain wall-clock time from an NTP/SNTP service when an Internet connection is available.

### 5.1 Requirements

- Synchronise after network connectivity becomes available.
- Retry after network outages.
- Periodically resynchronise.
- Support configurable NTP server names.
- Initial suggested servers: `pool.ntp.org`, `time.cloudflare.com` and `time.google.com`.
- Maintain time between synchronisations using the system clock.
- Use a monotonic clock for elapsed-time measurements, retry intervals and timeout scheduling.
- Store and exchange timestamps as UTC/Unix epoch values.
- Convert to local time only for display.

The system must explicitly represent an unsynchronised clock after cold boot. It must not fabricate a valid-looking timestamp when time is unknown.

NTP availability is not a prerequisite for LoRa reception or message buffering.

### 5.2 Timestamp semantics

The data model shall distinguish:

1. **Sensor timestamp:** when the measurement was made by the originating node, if supplied and trustworthy.
2. **Gateway receive timestamp:** when the gateway received the packet.
3. **Server ingest timestamp:** when the PHP backend accepted the message.

The gateway should preserve the source timestamp and its validity/quality indication. It must not silently replace a missing sensor timestamp with the gateway timestamp.

NTP is an operational time reference, not an authentication or security authority.

## 6. Telemetry processing and HTTPS backend

### 6.1 Backend interface

The gateway shall forward telemetry directly to the existing PHP HTTPS service at:

`telemetry.philipmcgaw.com`

The current hosting arrangement is Hetzner shared hosting with PHP and managed MySQL. The design must not require installation or administration of a public MQTT broker.

The gateway shall use HTTPS POST. MQTT is not part of the selected gateway-to-server transport.

The exact endpoint path, request schema, authentication protocol, response schema and server-side rate limits remain to be agreed and documented before implementation.

### 6.2 Payload handling

LoRa packets may use a compact binary representation to minimise airtime. The gateway shall decode or wrap received data into a versioned Internet representation suitable for HTTPS submission.

The backend representation should carry, where available:

- Schema version.
- Network identifier.
- Source node identifier.
- Message/sequence identifier.
- Sensor timestamp and validity.
- Gateway identifier.
- Gateway receive timestamp.
- Radio RSSI and SNR.
- Decoded measurements and units.
- Original raw packet or an encoded representation of it.

The raw packet should be retained alongside decoded data where practicable, to support diagnostics and future decoder improvements.

The exact JSON schema is an open design item; field names and units must be agreed before the PHP endpoint is implemented.

### 6.3 Authentication and transport security

- Use HTTPS with certificate and hostname validation.
- Do not disable TLS certificate validation to work around connectivity problems.
- Authenticate the gateway using a per-device credential or a keyed message-authentication mechanism such as HMAC-SHA-256.
- Keep device credentials out of source control and diagnostic logs.
- Support credential rotation and revocation.
- Consider replay protection using message identifiers, sequence numbers and authenticated request data.

The final credential-provisioning and key-management approach remains open.

### 6.4 Acknowledgement and idempotency

The gateway must retain a queued message until the server confirms successful ingestion.

The PHP backend must validate messages and provide an unambiguous acknowledgement identifying accepted messages or their sequence range.

The backend must be idempotent: retrying a message after a lost response must not create duplicate measurement records.

A mesh radio acknowledgement and a server-ingestion acknowledgement are different events and must not be conflated.

The server should record its own ingest timestamp.

## 7. Persistent telemetry queue

### 7.1 Purpose

The queue protects telemetry against temporary Internet, DNS, TLS and server outages, including gateway reboot or power interruption.

### 7.2 Proposed behaviour

- Persist received messages before considering them safely queued.
- Preserve message identity and ordering information.
- Retry forwarding when backend connectivity returns.
- Remove or mark records complete only after a valid server acknowledgement.
- Recover queued and in-flight records after reboot.
- Prevent duplicate database ingestion through server-side idempotency.
- Expose queue depth and oldest-message age to the OLED and diagnostics.
- Apply bounded retry/back-off behaviour to avoid excessive network requests.

The initial queue allocation target is approximately 1 MiB, subject to partition-layout verification.

A planning example of 256 bytes per stored record gives approximately 4,096 records per MiB before filesystem and implementation overhead. Actual capacity must be measured using the final record format and filesystem.

### 7.3 Queue exhaustion

The system must define and expose behaviour when the queue is full. It must not silently overwrite unforwarded messages.

The policy should prioritise preserving existing unacknowledged telemetry and report queue exhaustion as a fault. Any deliberate data-discard policy requires an explicit design decision.

### 7.4 Buffering target

The proposed initial resilience target is 48–72 hours without Internet access. This is provisional and must be checked against expected node count, reporting intervals, payload size and queue overhead.

## 8. OLED operational display

The integrated OLED shall provide a useful local operational dashboard rather than only boot or debug messages.

Proposed display pages:

**Page 1 — Network**
- Active interface: Wi-Fi or Ethernet.
- Connected Wi-Fi SSID, when applicable.
- Wi-Fi RSSI in dBm and graphical indication.
- IP address.
- Connectivity state.

**Page 2 — Mesh**
- Mesh/protocol state.
- Received and transmitted packet counters, where available.
- Latest packet RSSI and SNR.
- Age of last received packet.

**Page 3 — Internet and queue**
- HTTPS/backend reachability.
- Last successful upload time.
- Pending queue count.
- Age of oldest queued message.
- Last upload error, if relevant.

**Page 4 — System**
- Uptime.
- Free heap.
- Firmware version.
- UTC time and synchronisation state.
- Last NTP synchronisation and source, where available.

Illustrative Wi-Fi RSSI descriptions:

| RSSI | Description |
|---:|---|
| -50 dBm or better | Excellent |
| Around -60 dBm | Good |
| Around -70 dBm | Fair |
| Around -80 dBm | Weak |
| Below -85 dBm | Very weak |

These are indicative display bands, not guaranteed performance thresholds.

A short button press should cycle display pages. A long press should enter configuration mode, with a separate deliberate action for factory reset. Double-press behaviour and display auto-rotation/timeout remain optional decisions.

Display updates must be asynchronous and must not block radio reception, queue handling or network processing.

## 9. OTA firmware updates

### 9.1 Requirement

The gateway shall support remote over-the-air firmware updates by downloading firmware from the project's HTTPS server.

OTA is a core design requirement. It must be included in the initial firmware architecture and flash partition planning.

### 9.2 Update discovery and policy

- The gateway shall periodically check a server-hosted firmware manifest.
- A manual update check shall be available through the local management interface.
- A proposed default check interval is once every 24 hours, configurable later.
- The manifest shall identify firmware version, hardware compatibility, firmware download location, image size and cryptographic digest.
- The gateway shall compare the available release with its installed version and hardware revision.
- Firmware updates should be staged: download and verify first, then install during a suitable maintenance window.
- Automatic installation should be configurable; manual installation must remain possible.

The manifest format, URL structure and release-management process are open design items.

### 9.3 Download and verification

- Download firmware over HTTPS.
- Validate the server certificate and hostname.
- Verify image size and SHA-256 digest.
- Verify a digital signature using a public verification key embedded in or securely provisioned to the device.
- Keep the signing private key off the gateway and out of the public web directory.
- Reject images with invalid signatures, hashes, sizes, hardware compatibility or version policy.
- Do not install an incomplete or partially downloaded image.

HTTPS protects the transport; image signing provides an additional check that the firmware was authorised by the project.

### 9.4 Installation and recovery

The firmware partition layout shall support dual application slots or an equivalent recoverable OTA arrangement.

Proposed sequence:

1. Check that the update is compatible.
2. Check available power and network conditions.
3. Complete or safely persist current message-processing work.
4. Download to the inactive firmware slot.
5. Verify the downloaded image.
6. Set the inactive slot as the next boot target.
7. Reboot.
8. Run defined post-boot self-tests.
9. Mark the new image valid only after successful checks.
10. Roll back to the previous working image if validation fails.

The self-test should cover core system startup, persistent queue access, mesh initialisation and network/HTTPS operation where available. Temporary Internet unavailability must not automatically invalidate otherwise healthy firmware.

The gateway should report update outcome and failure reason to the backend when connectivity permits.

Initially, OTA should update application firmware only. Bootloader, partition-table and security-configuration changes are separate, higher-risk maintenance operations.

### 9.5 Update safety

- Never erase the currently working application image before the replacement is verified.
- Do not discard the telemetry queue during an update.
- Do not require Internet connectivity for normal LoRa reception.
- Avoid installation while the gateway is processing a critical operation.
- Provide a way to disable automatic installation while retaining manual update capability.
- Record installed version, attempted version, update time and result.

## 10. Firmware architecture

The firmware should be divided into independent services with defined interfaces.

| Module | Responsibility |
|---|---|
| `NetworkManager` | Wi-Fi/Ethernet connectivity, selection and failover |
| `ProvisioningManager` | Captive portal and network configuration |
| `NTPTimeManager` | UTC time synchronisation and clock state |
| `MeshAdapter` | Protocol-specific mesh interface |
| `TelemetryManager` | Packet validation, decoding and metadata |
| `QueueManager` | Persistent telemetry buffering and recovery |
| `HTTPSClient` | Authenticated backend communication |
| `OTAUpdateManager` | Manifest checking, download, verification and installation |
| `DisplayManager` | OLED dashboard |
| `StatusLEDManager` | External RGB status indication |
| `SystemMonitor` | Watchdog, diagnostics, uptime and resource monitoring |

The choice between ESP-IDF and Arduino-ESP32 remains open. The decision should be informed by mesh firmware integration, network-interface requirements, TLS support, OTA rollback support and maintainability. ESP-IDF capabilities should be preferred where they materially improve reliability or security.

## 11. Reliability and fault handling

The gateway should remain capable of receiving and buffering LoRa messages during:

- Wi-Fi loss.
- Ethernet loss.
- DNS failure.
- NTP unavailability.
- TLS handshake failure.
- Backend outage or HTTP error.
- OTA manifest or download failure.

Network and backend operations must use bounded timeouts and retry/back-off policies. They must not block mesh reception indefinitely.

A watchdog should recover from genuine firmware lock-up. Recovery must preserve persistent configuration and queued telemetry.

Diagnostics should distinguish at least:

- No mesh reception.
- Mesh adapter failure.
- Wi-Fi provisioning required.
- Wi-Fi disconnected.
- Ethernet disconnected.
- No Internet/backend reachability.
- NTP unsynchronised.
- HTTPS authentication or TLS failure.
- Queue warning/full.
- OTA available, in progress, failed or successful.
- Critical system fault.

## 12. Security

The design shall consider:

- Unique gateway identity.
- Per-device backend credentials.
- TLS certificate and hostname validation.
- Signed firmware images.
- Secure handling of signing and authentication keys.
- Protected provisioning access point.
- No plaintext passwords in logs.
- Input validation for received mesh data and backend responses.
- Replay and duplicate-message handling.
- Firmware version and compatibility policy.
- Secure Boot and flash encryption as future security-hardening options, subject to their effect on development, recovery and key management.

The final threat model and security configuration remain open design items.

## 13. Development stages and acceptance criteria

The following staged development plan is proposed.

| Stage | Objective | Acceptance evidence |
|---|---|---|
| G1 | Hardware bring-up | Heltec boots; OLED and Wi-Fi operate |
| G2 | Mesh integration | Valid mesh telemetry received on the selected board |
| G3 | Telemetry decoding | Source node, message identity and measurements extracted |
| G4 | Persistent queue | Messages survive network outage and gateway reboot |
| G5 | HTTPS forwarding | Authenticated submission and valid acknowledgement |
| G6 | Backend ingestion | PHP validates and stores telemetry |
| G7 | Recovery and idempotency | No duplicate database records after retries; queued data recovers |
| G8 | Field trial | Range, packet-delivery ratio and latency measured |
| G9 | Provisioning and diagnostics | Captive portal, OLED and RGB status behaviour verified |
| G10 | Time service | NTP synchronisation and unsynchronised operation verified |
| G11 | OTA | Valid update installs; interrupted/invalid update is rejected or rolled back |
| G12 | Ethernet option | Ethernet operation and Wi-Fi failover demonstrated, when hardware is added |

Additional tests should cover malformed packets, TLS interruption, HTTP errors, queue exhaustion, power loss during queue writes, watchdog recovery, network failover, NTP loss, credential changes and OTA power interruption.

The system should not be described as validated or production-proven until the relevant acceptance tests have been completed and recorded.

## 14. Open design decisions

The following items require investigation or agreement before implementation is frozen:

1. Final mesh protocol: MeshCore or Meshtastic.
2. Exact mesh firmware/companion integration on the Heltec V3.
3. Firmware framework: ESP-IDF or Arduino-ESP32.
4. Confirmed flash/PSRAM capacity and OTA partition table.
5. Queue record format, actual storage allocation and measured outage capacity.
6. Whether the 48–72-hour buffering target is sufficient for the intended node population.
7. Ethernet controller and TLS/network-stack compatibility.
8. Backend endpoint path, payload schema and acknowledgement format.
9. Gateway authentication and key-provisioning method.
10. Firmware manifest format, signing process and update policy.
11. Final button behaviour and OLED page controls.
12. Exact UK radio configuration and regulatory limits.
13. Whether Secure Boot and flash encryption are required for the first deployable release.

## 15. Current project status

This document records the proposed architecture and requirements discussed to date.

**Selected for initial development:**
- Heltec WiFi LoRa 32 V3.
- Existing mesh protocol approach.
- MeshCore as the first protocol candidate for evaluation.
- Direct HTTPS forwarding to the existing PHP backend.
- Internal-flash telemetry queue as the initial storage approach.

**Planned provisions:**
- Captive portal Wi-Fi provisioning.
- Optional Ethernet.
- External addressable RGB status LED.
- OLED operational dashboard.
- NTP time synchronisation.
- Signed, recoverable OTA firmware updates.

**Not yet implemented or validated:**
- Gateway firmware.
- Mesh protocol integration.
- Persistent queue.
- HTTPS endpoint integration.
- OTA update service.
- Ethernet interface.
- Final hardware/PCB changes.

All hardware and software selections described as candidates or proposals remain subject to verification and review.
