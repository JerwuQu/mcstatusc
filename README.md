# mcstatusc
Minecraft Server Status thingy

[![Build status](https://ci.appveyor.com/api/projects/status/g9gjuf6fk3fxsgk1?svg=true)](https://ci.appveyor.com/project/JerwuQu/mcstatusc)

# Why?
I was previously using [Dinnerbone's mcstatus](https://github.com/dinnerbone/mcstatus) but it was quite slow, especially in Termux on my phone, taking multiple *seconds* to simply get the status... so here we are, and now it's fast.

# Usage
`mcstatusc <hostname> [port]`

## Example
```
$ mcstatusc 127.0.0.1 | jq
{
  "description": {
    "text": "A Minecraft Server"
  },
  "players": {
    "max": 20,
    "online": 1,
    "sample": [
      {
        "id": "REDACTED",
        "name": "REDACTED"
      }
    ]
  },
  "version": {
    "name": "1.14.3",
    "protocol": 490
  }
}
```
[jq](https://github.com/stedolan/jq), incase you don't have it already.

# Download
## Arch
I supplied a `PKGBUILD` in this repo for now. If this ever gets more than 0 users, I'll try to throw it onto the AUR.

## Windows
A binary is built for each commit [on AppVeyor](https://ci.appveyor.com/project/JerwuQu/mcstatusc/build/artifacts).

## Others
Build it. See below.

# Building on Linux
- Install `gcc` for your distribution
- Run `make`

# Building on Windows
- Download [The Tiny C Compiler](https://bellard.org/tcc/) by [Fabrice Bellard](https://bellard.org/) and add to your `PATH`
- Run `make.bat`

# License
MIT. See `LICENSE` in this repository for more info.
