# VUMZ (VU Meter visualiZer)

| <code>vumz</code> is a lightweight command-line VU meter visualizer designed for unix systems. It captures audio using <code>PipeWire</code> and displays the audio levels using <code>ncurses</code> in an ASCII-based interface. | ![vumz](doc/vumz.gif "vumz") |
|---|---|


## Features
- Real-time audio level visualization: Captures and visualizes audio in real time using ascii characters.
- Dynamic smoothing and noise reduction: Built-in smoothing functions help stabilize the display.
- Responsive Design: Adapts to the initial terminal size to make efficient use of the available space. 
- Color themes: 7 distinct color themes designed to align with the terminal's color scheme.

## Installation

To install vumz, you need to have `ncurses` and `pipewire` installed on your system. You can install them using your package manager.

### Fedora
```bash
sudo dnf install pipewire pipewire-devel ncurses ncurses-devel
```

### Ubuntu (22.04+)
```bash
sudo apt install libncurses5-dev libncursesw5-dev libpipewire-0.3-dev
```

### Arch
```bash
sudo pacman -S pipewire ncurses
```

### Void
```bash
sudo xbps-install -S pipewire-devel ncurses-devel
```

// TODO: Add instructions for other distributions

### Building from source

Once you have the dependencies installed, you can clone the repository and install the program using the following commands:

##### Using the script
```bash
cd vumz/
chmod +x install.sh
./install.sh
# To uninstall vumz
chmod +x uninstall.sh
./uninstall.sh
```

##### Or using the Makefile
```bash
cd vumz/
make
sudo make install
# As for uninstalling vumz use the following command
sudo make uninstall
```

## Usage

```
Usage: vumz [OPTION]...

vumz is a simple cli vumeter.

Options:
    -D, --debug         debug mode: print useful data
    -h, --help          show help
    -S, --screensaver   screensaver mode: press any key to quit

Keys:
    Left        Switch to previous color theme
    Right       Switch to next color theme
    Up          Increase noise reduction
    Up          Decrease noise reduction
    d           Toggle debug mode
    q/Escape    Quit
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


The decibel value is then smoothed out by comparing it to the previous captured value using the following function:

```c
/*
 * This smoothing function was adapted from cava:
 * https://github.com/karlstav/cava/blob/master/cavacore.c
 */
void apply_smoothing(float* channel_dbs, struct audio_data* audio, int buffer_index) {
    float previous_dbs = audio->audio_out_buffer_prev[buffer_index];

    if (*channel_dbs < previous_dbs) {
        *channel_dbs = previous_dbs * (1.0 + (audio->fall[buffer_index] * audio->fall[buffer_index] * 0.03));
        audio->fall[buffer_index] += 0.98;
    }
    else {
        audio->peak[buffer_index] = *channel_dbs;
        audio->fall[buffer_index] = 0.0;
    }

    audio->audio_out_buffer_prev[buffer_index] = *channel_dbs;

    *channel_dbs = audio->mem[buffer_index] * 0.2 + *channel_dbs;
    audio->mem[buffer_index] = *channel_dbs;
    audio->audio_out_buffer[buffer_index] = *channel_dbs;
}
```

Once the signal has been smoothed out, the program uses `ncurses` to draw the VU Meter.

## Man Page

The man page for `vumz` provides ~~not so~~ detailed information about the program.

To view it, run:

```bash
man vumz
```

## Acknowledgements

Some of the code for ncurses was modified from [NCURSES Programming HOWTO](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/index.html) and other parts were inspired by [cbonsai](https://gitlab.com/jallbrit/cbonsai).

Some of the PipeWire code was taken directly from the [audio-capture example](https://docs.pipewire.org/audio-capture_8c-example.html#a9) in their documentation.

The smoothing function and the `audio_data` struct were adapted from [cava](https://github.com/karlstav/cava).

## Inspiration

After playing around with [cava](https://github.com/karlstav/cava) and [cbonsai](https://gitlab.com/jallbrit/cbonsai), I thought it would be fun to make a simple program to capture audio from my computer and visualize it in real-time.

## TODO Ideas

- [x] Smooth out the VU meter animation
- [x] Add more color options
- [ ] Adjust sensitivity
- [ ] Make the rendering more "fancy" by adding stuff such as "particles" or a 3D effect.

