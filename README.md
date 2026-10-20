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

### Rendering with nxdk-gles11

**This Xbox port builds and links [Ryzee119's nxdk-gles11](https://github.com/Ryzee119/nxdk-gles11)
as its active graphics backend.** It is a runtime dependency compiled into the
XBE. Ryzee119 deserves credit for the GLES implementation that makes this
rendering path possible.

Neverball already has an OpenGL ES rendering path. We compile that path with
`ENABLE_OPENGLES=1`; nxdk-gles11 implements its GLES calls for the original
Xbox's NV2A GPU through NXDK/pbkit:

**Neverball's GLES renderer → nxdk-gles11 → NXDK/pbkit → Xbox NV2A GPU**

This lets the modern game reuse its existing rendering code for level geometry,
textures, lighting, fog, vertex buffers, shadows and particle effects on Xbox.
The backend also supplies the graphics-context and frame-presentation hooks
used by the Xbox platform code. Together, these enable the modern levels,
ball and coin rendering, title replay visuals, and portal effects seen in our
physical hardware tests. Controller input, audio and the Xbox system menu are
handled by the game's separate platform integration.

The integration is concrete and reproducible:

* [Renderer bootstrap](scripts/bootstrap-xbox-renderer.py) fetches the pinned
  nxdk-gles11 revision `0cf17fa709f42158d11a1571f87705b692699bd3`.
* [Xbox CMake configuration](xbox/CMakeLists.txt) compiles the renderer, and
  [Makefile.nxdk](Makefile.nxdk) links `GLESv1_CM`, `swizzle` and `arena` into the
  game alongside pbkit. We use GLES directly; the optional gl4es layer is disabled.
* [Xbox platform code](xbox/platform.c) calls its `glContextInit` and `glFlipNV2A`
  hooks to initialize graphics and present frames.
* [The portal-particle fix](https://github.com/ZacharyLeahan/neverball-nxdk/issues/6)
  adapts Neverball's base-texture assignment to the backend's NV2A point-sprite
  requirement: texture unit 3. Goal stars and teleporter sparkles were then
  verified on the physical Xbox; before/after captures are in the issue.

See the [port notes](doc/nxdk.md) for backend constraints, dependency attribution
and build instructions. The game currently requires a **128 MiB Xbox**;
using this renderer does not establish stock 64 MiB support.

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
