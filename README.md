# gk-oscillator

**Graphic oscillator** for Linux — tone generator + selectable sound input, with a real-time **oscilloscope** display.

**Single binary** — no app plugins, no DLL/shared objects for the app itself (system Qt and PortAudio are linked normally).

## Features

- Large dark **scope** view (generator / input / mix)
- **Selectable input and output** devices (PortAudio → ALSA / Pulse / JACK / PipeWire)
- Waveforms: sine, square, saw, triangle, noise
- Frequency (20–20 kHz), amplitude, generator on/off
- Optional **input monitor** (route capture to output at reduced gain)
- Timebase, vertical display gain, freeze
- Settings saved to `~/.config/gk-oscillator/config`

## Requirements

- C11 + C++17 (`gcc` / `g++`)
- **Qt 6** Widgets (`qt6-base-dev`, `pkg-config Qt6Widgets`)
- **PortAudio** runtime (`libportaudio2`); optional `portaudio19-dev` for system headers  
  (this tree vendors `third_party/portaudio.h` so the runtime library alone is enough to link)

```bash
# Debian/Ubuntu
sudo apt install qt6-base-dev libportaudio2
# optional for system headers:
sudo apt install portaudio19-dev libasound2-dev
```

## Build

```bash
make
./gk-oscillator
```

```bash
make install-user    # ~/.local/bin + desktop entry
make desktop-local   # ./gk-oscillator.desktop next to the binary
make clean
```

## Usage

1. Pick **Output** (and optionally **Input**).
2. Press **Start**.
3. Adjust waveform / frequency / amplitude — the scope is the main display.
4. Set **Source** to Input or Mix to view the capture path.
5. **Freeze** pauses the display only; audio keeps running.

Default amplitude is **0.2** so a tone is audible without blasting.

## Layout

| Path | Role |
|------|------|
| `src/audio.c` | PortAudio streams, oscillator, device enum |
| `src/ringbuf.c` | Display sample rings |
| `src/scope_widget.cpp` | Oscilloscope paint |
| `src/main_window.cpp` | Controls + layout |
| `src/config.c` | Key=value config |
| `third_party/portaudio.h` | Vendored PA API header |

## Non-goals (v1)

- Not a VST/LV2/CLAP plugin
- No TUI / headless / IPC (yet)
- No spectrum analyzer, MIDI, or multi-voice synth

## Smoke checklist

- [ ] `make` produces one `gk-oscillator` binary
- [ ] Device lists populate; Refresh works
- [ ] Start → sine @ 440 on speakers/headphones
- [ ] Scope draws a stable wave; timebase/gain change the view
- [ ] Input device + Source=Input shows live capture (if a mic exists)
- [ ] Stop / Start / device change does not crash
- [ ] Quit restores geometry and last settings on relaunch

## License

MIT — see [LICENSE](LICENSE).
