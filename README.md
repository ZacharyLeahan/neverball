# neverball-nxdk

![Neverball](https://neverball.org/images/shots/01-neverball-easy/easy-07-01.jpg)

Tilt the  floor to roll a  ball through an obstacle  course within the
given  time.  If  the  ball falls  or time  expires, a ball is lost.

Collect coins to unlock the exit  and earn extra balls.  Red coins are
worth 5.  Blue coins are worth 10.  A ball is awarded for 100 coins.

## Original Xbox development

The modern game runs on a physical **128 MiB original Xbox** using NXDK and
[Ryzee119's nxdk-gles11](https://github.com/Ryzee119/nxdk-gles11). Local hardware testing confirmed playable Easy level 3,
correct coin rendering, responsive controls, music and effects. Automatic title
replay and repeated restarts were also observed. **64 MiB support is unverified.**
Easy 3 access in the isolated test installation uses a backed-up test unlock;
this is not a claim of earned progression or level completion.

See [build and port notes](doc/nxdk.md) and the
[physical hardware ledger](doc/hardware-testing.md) for commands, dependency
pins, exact tested XBE hashes and remaining limitations.

Xbox **Exit** offers confirmed launcher return, game restart,
console reboot and power off, with Cancel selected by default. See the
[system-menu notes](doc/xbox-exit-menu.md) for configuration and test coverage.

This Xbox continuation builds on [JayFoxRox / Jannik Vogel's original NXDK
port](https://github.com/JayFoxRox/neverball/tree/xbox). The modern game retains
upstream Neverball history, content and contributor credits, and
uses [Ryzee119's nxdk-gles11](https://github.com/Ryzee119/nxdk-gles11) renderer.

### Source and reference repositories

* [This modern continuation](https://github.com/ZacharyLeahan/neverball-nxdk).
* [Original Neverball upstream](https://github.com/Neverball/neverball) and
  [Neverball contributors](doc/authors.txt): game, engine, levels and assets.
* [JayFoxRox / Jannik Vogel's original Xbox port](https://github.com/JayFoxRox/neverball/tree/xbox):
  original Xbox/NXDK port authorship and the foundation of this continuation.
* [Ryzee119/nxdk-gles11](https://github.com/Ryzee119/nxdk-gles11): the direct
  OpenGL ES renderer used by this migration.
* [XboxDev/NXDK](https://github.com/XboxDev/nxdk): toolchain, runtime and pbkit.
* [ESP KVM Xbox project](https://github.com/ZacharyLeahan/espkvm-xbox): physical
  video/audio capture and remote controller testing.

Inherited code retains its original notices; adaptations and source commits
are credited in the port notes. Dependency notices remain with their sources.

## Release Notes

Release highlights can be found in [doc/release-notes.md](doc/release-notes.md).

## Documentation

* [LICENSE.md](LICENSE.md): a description of licensing and exceptions
* [doc/install.txt](doc/install.txt): instructions on how to build the
  game from source code
* [doc/manual.txt](doc/manual.txt): a detailed description of how to
  play and configure the game
* [doc/authors.txt](doc/authors.txt): a list of people who have
  contributed to Neverball

## Resources

* [Website](https://neverball.org/)
* [Development](http://github.com/Neverball)
* [Discord](https://discord.gg/HhMfr4N6H6)

## Translation

Neverball uses the gettext approach to translations. We're always
interested in covering more languages. Join the
[Neverball project on Transifex](https://www.transifex.com/neverball/neverball/).
