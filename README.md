# statfile

![statfile screenshot](./screenshot.png)

statfile is a macOS program that writes various system stats to a file and
allows you to easily access them by simply reading the file.

statfile currently provides the following stats:

- Network traffic
- CPU usage
- RAM usage
- Uptime
- Battery status
- Date and time

## Why?

I wrote this to use as a cheap source of stats in my custom Kitty terminal tab
bar.

Also, compared to something like [Stats](https://github.com/exelban/stats),
statfile uses virtually no resources to run even on a 1s update interval.

## Install

Clone/download the repo and run the install script `./install`, this will
install statfile in the `/usr/local/bin` directory.

To uninsall, run `./uninstall`.

## Usage

To start the program run `statfile start`.

By default the update interval is 1 second, if you want to change it to 5
seconds for example, run with the `-s` flag like this: `statfile start -s 5`.

To stop the program run `statfile stop`.

The program writes the stats to a file located at `~/.statfile` and its content
looks like this:

```text
<upload>|<download>|<uptime days>,<uptime hours>|<battery status>,<battery percent>|<date>|<time>|<cpu>|<ram>
```

Where a battery status of 1 means the battery is plugged and 0 means it's
unplugged.

Here is a concrete example:

```text
2KB|9KB|5,19|1,56|Mon 9 Oct|22:51|76|7
```
