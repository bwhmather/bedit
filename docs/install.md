gedit installation by building the source code
==============================================

gedit uses the [Meson](https://mesonbuild.com/) build system.

### Installation from Git

During gedit development, gedit may depend on a development version of a GNOME
dependency. To install gedit plus its GNOME dependencies all from Git, see the
file `docs/gedit-development-getting-started.md`.

### Installation from a tarball for a stable version

Simple procedure to install gedit onto the system:

```
$ meson builddir && cd builddir # Build configuration
$ ninja                         # Build
[ Become root if necessary ]
$ ninja install                 # Installation
```

If certain dependencies are too old, see the procedure to install from Git.
