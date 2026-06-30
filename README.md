# qo100

Schlanker SSB-Transceiver für den Amateurfunk-Satelliten **QO-100** (Es'hail-2)
mit **ADALM-Pluto** unter Linux. Ziel: robustes USB-SSB-Senden und -Empfangen
mit moderner UI — ohne den Funktionsballast großer SDR-Suiten.

## Status

| Phase | Inhalt | Stand |
|-------|--------|-------|
| 0 | Hardwarefreier SSB-Kern (USB mod/demod) + Selbsttest | **fertig** |
| 1 | Empfang: Tuning, AGC, Audio-Out, Wasserfall | offen |
| 2 | Senden: Full-Duplex über Pluto, PTT, gekoppelte TX/RX-Frequenz | offen |
| 3 | Robustheit: Beacon-Kalibrierung, Persistenz, UI-Politur | offen |

Der DSP-Kern wird **ohne Hardware** entwickelt und getestet (synthetische IQ
sowie später echte QO-100-Aufnahmen). Der Pluto-Adapter kommt erst, wenn ein
SDR zum Testen vorhanden ist.

## Funkstrecke (Schmalband-Transponder)

| | Uplink (TX) | Downlink (RX) |
|---|---|---|
| Frequenz | 2400.000–2400.500 MHz | 10489.500–10490.000 MHz |
| Offset | `f_down = f_up + 8089.5 MHz` (nicht invertierend) | |

- RX-Kette: Spiegel → LNB (10.5 GHz → ZF ~739 MHz) → Pluto RX
- TX-Kette: Pluto TX (2.4 GHz direkt) → PA (~1–5 W) → Patch/POTY im Spiegelfokus
- Pluto kann Full-Duplex mit unabhängigen RX/TX-LOs → man hört sich selbst.
- Doppler ist vernachlässigbar (geostationär); dominant ist die **LNB-LO-Drift**.
  Kalibrierung erfolgt über den mittleren PSK-Beacon @ 10489.750 MHz.

## Architektur

```
engine/   headless DSP, hardwarefrei testbar (IQ rein/raus)
  ssb.*     Hilbert-FIR, USB-Modulator/-Demodulator (phasing method)
  dsp.h     Signalgeneratoren + DFT-Bin-Messung für Tests
tests/    Selbsttests (modulate -> demodulate -> verify)
ui/       (Phase 1+) Dear ImGui Wasserfall + Bedienpanels
```

## Bauen & Testen

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure   # oder: ./build/test_ssb
```

## Geplante Abhängigkeiten (später)

- `libiio` — Pluto-Zugriff (USB & Netzwerk)
- `Dear ImGui` + `ImPlot` (Submodule) — GPU-Wasserfall und UI
- Audio-I/O (`libsoundio`/`RtAudio`)
- `liquid-dsp` ist installiert, wird aber für den Kern nicht benötigt.

## Test-IQ ohne eigene Hardware

QO-100-IQ-Aufnahmen für die Entwicklung gibt es u. a. über den
BATC/AMSAT-DL WebSDR (eshail.batc.org.uk) und diverse öffentliche Mitschnitte.
Bis dahin liefert der eingebaute Signalgenerator deterministische Testsignale.
