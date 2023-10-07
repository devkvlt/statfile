# statfile

statfile is a macOS program that writes various system stats to a file and allows
you to easily access them by simply reading the file.

## Why?

I wrote this to use as a cheap source of stats in my custom Kitty terminal tab
bar.

## Install

Download the code and run the install script `./install`.

## Usage

`statfile start` to start the program and `statfile stop` to stop it.

The program writes the stats to a file located at `~/.statfile` and its content
looks like this:

```text
0,0|0,0|0,8|0,50|17/06/2023|03:50|78
```

## TODO

Explain what each of the fields means.
