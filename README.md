# Quickshot

A small C++20 and Qt 6 Widgets project template with a reproducible Nix development and build
environment.

## Quick start

Run the application directly:

```console
nix run
```

Enter the development shell and build with CMake:

```console
nix develop
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/quickshot
```

With direnv installed, run `direnv allow` once. Entering the directory then loads the same
development shell automatically and installs the configured Git hooks.

## Image controls

Use **Open** to load an image. The rotate-left and rotate-right actions become available after an
image loads successfully. Hold **Ctrl** and use the mouse wheel to zoom from the image's top-left
corner, or edit the toolbar factor with its arrows or mouse wheel. The zoom range is 10% to 800%.
A normal mouse wheel over the image continues to scroll it. Open and ROI save dialogs remember
their most recently used directories independently.

Use **Rectangle** or **Ellipse** to draw image-coordinate ROIs. Drag a shape to move it or use one of
its eight handles to resize it. The selected shape is red and unselected shapes are white.
Left-drag any handle to resize, or right-drag any handle to rotate around the shape's center.
Right-click a shape to save, clone, or delete it. Right-click outside all shapes to save or delete
all ROIs. Saved files use PNG; ellipse pixels outside the shape are transparent.

## Nix outputs

```console
nix build                              # packages.default
nix run                                # apps.default
nix develop                            # devShells.default
nix flake check                        # package, tests, and pre-commit checks
nix fmt                                # run all formatting and lint hooks
nix run .#generate-compile-commands    # configure clangd/IDE metadata
```

The template is also exposed as a flake template. From another directory, use
`nix flake init -t path:/path/to/quickshot`.

## Git hooks

Entering `nix develop` installs formatting, Nix linting, typo, whitespace, merge-conflict, and
large-file checks. Clang-tidy runs at the `pre-push` stage because it needs a CMake compilation
database. Run everything manually with:

```console
pre-commit run --all-files
pre-commit run clang-tidy --all-files --hook-stage pre-push
```
