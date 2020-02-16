# States Machine
**States Machine** is an app to memorize the names and locations of the fifty US states
using [spaced repetition](https://en.wikipedia.org/wiki/Spaced_repetition).

![Example](example.png)

# Platforms
| OS           | Compiler                 | Maintainer                             | Notes                            |
| -            | -                        | -                                      | -                                |
| MacOS        | `llvm`                   | [@mkeeter](https://github.com/mkeeter) | Main development platform        |
| Windows      | `x86_64-w64-mingw32-gcc` | [@mkeeter](https://github.com/mkeeter) | Compiles, but doesn't quite work |
| Your OS here | `???`                    | Your username here                     | Contributors welcome!            |

Other platforms will be supported if implemented and maintained by other contributors.

To become a platform maintainer, open a PR which:
- Implements a new platform
- Add details to the table above
- Updates the **Compiling** instructions below.

# Compiling
At the moment, **States Machine** supports compiling a native application on MacOS,
or cross-compiling to Windows (if `TARGET=win32-cross` is set).

## Building dependencies
GLFW is shipped in the repository, to easily build a static binary.  It only needs to be compiled once.
```
[env TARGET=win32-cross] make glfw
```

## Building
```
[env TARGET=win32-cross] make
```

## Deploying an application bundle
### MacOS
[`cd deploy/darwin && deploy.sh`](https://github.com/mkeeter/states-machine/blob/master/deploy/darwin/deploy.sh)
