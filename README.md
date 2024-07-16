# VUMZ (VU Meter visualiZer)

| <code>vumz</code> is a simple command-line VU meter visualizer for unix systems. It uses <code>ncurses</code> and <code>PipeWire</code> to display the audio levels of the left and right audio channels. | ![vumz](doc/vumz.gif "vumz") |
|---|---|


## Features
- Real-time audio level visualization

## Installation

To install vumz, you need to have `ncurses` and `pipewire` installed on your system. You can install them using your package manager.

### Fedora
```bash
sudo dnf install pipewire pipewire-devel ncurses ncurses-devel
```

// TODO: Add instructions for other distributions

### Building from source

Once you have the dependencies installed, you can clone the repository and install the program using the following commands:

```bash
chmod +x install.sh
./install.sh
```

To uninstall the program, you can run the following commands:

```bash
chmod +x uninstall.sh
./uninstall.sh
```

## Usage

```
Usage: vumz [OPTION]...

vumz is a simple cli vumeter.

Options:
    -D, --debug                debug mode: print useful data for devs.
    -h, --help                    show help.
    -S, --screensaver     screensaver mode: press any key to quit.
```
## How it works

vumz captures audio data using [PipeWire](https://pipewire.org/), a low-level multimedia framework. The audio data is processed to calculate the maximum amplitude in the left and right channels. The amplitude is then converted to (dB) using the following function:

```c
static float amplitude_to_db(float amplitude)
{
    if (amplitude <= 0.0f)
    {
        return -60.0f;
    }

    return 20.0f * log10f(amplitude);
}
```

This function was ~~stolen from some random stackoverflow post I don't remember~~ derived from the formula `db = 20 * log10(amplitude)`.

The dB value is then used to draw the VU meter using `ncurses`.

## Man Page

The man page for `vumz` provides ~~not so~~ detailed information about the program.

To view it, run:

```bash
man vumz
```

## Acknowledgements

Some of the code for ncurses was modified from [NCURSES Programming HOWTO](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/index.html) and other parts were inspired by [cbonsai](https://gitlab.com/jallbrit/cbonsai).

Some of the PipeWire code was taken directly from the [audio-capture example](https://docs.pipewire.org/audio-capture_8c-example.html#a9) in their documentation.

## Inspiration

After playing around with [cava](https://github.com/karlstav/cava) and [cbonsai](https://gitlab.com/jallbrit/cbonsai), I thought it would be fun to make a simple program to capture audio from my computer and graph it in real-time.

## TODO Ideas

- [ ] Smooth out the VU meter animation
- [ ] Add more color options
- [ ] Add sensitivity option

