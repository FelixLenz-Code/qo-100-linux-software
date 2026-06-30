# qo100

Schlanker SSB-Transceiver für den Amateurfunk-Satelliten **QO-100** (Es'hail-2)
mit **ADALM-Pluto** unter Linux. Ziel: robustes USB-SSB-Senden und -Empfangen
mit moderner UI — ohne den Funktionsballast großer SDR-Suiten.

## Status

| Phase | Inhalt | Stand |
|-------|--------|-------|
| 0 | Hardwarefreier SSB-Kern (USB mod/demod) + Selbsttest | **fertig** |
| 1a | IQ-Datei-I/O (.cf32) + RX-DSP (Tuning, Dezimierung, AGC, Demod) | **fertig** |
| 1b | WAV-Ausgabe, FFT/Spektrum-Backend, CLI-Tool (`qo100_cli`) | **fertig** |
| 1c | Wasserfall-GUI (Datei-Viewer: Spektrum, Tuning, Decode→WAV) | **fertig** |
| 1d | Live-Audio (Soundkarte, Echtzeit-Streaming) | offen |
| 2a | TX-DSP (USB-Mod, Interpolation, Up-Mix) + TX→RX-Loopback-Test | **fertig** |
| 2b | Pluto-Anbindung (libiio), Full-Duplex, PTT, gekoppelte TX/RX-Frequenz | offen |
| 3a | Beacon-Kalibrierung (LNB-Drift messen) + QO-100-Frequenzplan | **fertig** |
| 3b | Persistenz der Einstellungen, UI-Politur, Kalibrierung in der GUI | offen |

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
  ssb.*       Hilbert-FIR, USB-Modulator/-Demodulator (phasing method)
  rx.*        RX-Kette: NCO-Tuner -> FIR-Dezimierung -> USB-Demod -> AGC
  tx.*        TX-Kette: USB-Mod -> FIR-Interpolation -> NCO-Up-Mix
  calib.*     Beacon-Kalibrierung: CW-Beacon finden, LNB-Drift messen
  qo100.h     Frequenzplan (Uplink/Downlink, Beacons, 8089.5-MHz-Offset)
  fft.*       abhängigkeitsfreie radix-2 FFT
  spectrum.*  Wasserfall-Zeile: Fenster -> FFT -> dBFS (fftshift)
  iqfile.*    .cf32-Aufnahmen lesen/schreiben (GNU Radio / SDR++ kompatibel)
  wavfile.*   16-bit-Mono-WAV lesen/schreiben
  dsp.h       Signalgeneratoren + DFT-Bin-Messung für Tests
apps/
  qo100_cli   CLI: Test-Szene erzeugen, .cf32 -> .wav dekodieren, WAV -> .cf32
ui/
  main.cpp    Wasserfall-GUI (Dear ImGui + ImPlot): Aufnahme laden, abstimmen,
              Decode -> WAV. Kein Audiogerät nötig.
extern/       Submodule: imgui, implot
tests/        Selbsttests (SSB, RX-Kette, TX-Loopback, FFT/Spektrum/WAV)
```

## CLI nutzen (ohne Hardware)

```sh
# synthetische Test-Szene erzeugen (USB-Signal + Störer + Rauschen)
./build/qo100_cli gen scene.cf32

# .cf32 dekodieren -> hörbare .wav (fsIn, Dezimierung, Tune-Offset in Hz)
./build/qo100_cli decode scene.cf32 384000 8 50000 out.wav

# Sendepfad: Mono-WAV USB-modulieren -> .cf32 (fsOut, Interpolation, Tune-Offset)
./build/qo100_cli modulate out.wav 384000 8 50000 tx.cf32

# LNB-Drift am Beacon messen (erwartete Position, Suchfenster in Hz)
./build/qo100_cli calibrate scene.cf32 384000 20000 8000
```

Echte QO-100-Mitschnitte (interleaved float32 `.cf32`, z. B. aus gqrx oder vom
BATC-WebSDR) funktionieren direkt mit `decode` — Abtastrate und Tune-Offset
entsprechend der Aufnahme angeben.

## Bauen & Testen

```sh
git submodule update --init --recursive       # imgui + implot für die GUI
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Die GUI braucht `glfw3` + OpenGL (Ubuntu: `libglfw3-dev`). Ohne diese Pakete
wird nur die Engine + CLI gebaut (`-DBUILD_UI=OFF` erzwingt das).

## GUI starten

```sh
./build/qo100_cli gen scene.cf32     # Testaufnahme, falls keine eigene da ist
./build/qo100_ui scene.cf32          # Wasserfall: abstimmen, Decode -> decoded.wav
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
