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

Use **Open** to append one or more images. A vertical thumbnail sidebar appears when multiple images
are open; each image keeps its own shapes, selection, and undo history. Right-click a thumbnail to
delete that image. The rotate-left and rotate-right actions become available after an image loads
successfully. Hold **Ctrl** and use the mouse wheel to zoom from the image's top-left corner, or edit
the toolbar factor with its arrows or mouse wheel. The zoom range is 10% to 800%. A normal mouse
wheel over the image continues to scroll it. Open and ROI save dialogs remember their most recently
used directories independently.

Use **Rectangle**, **Ellipse**, **Circle**, **Polygon**, or **Bezier Curve** to draw image-coordinate
ROIs. Polygon and Bezier Curve connect left-clicked anchors automatically; right-click finishes and
closes the shape without adding another anchor. Finish a
polygon by right-clicking after adding at least three vertices with the left button. Drag a shape to
move it or use one of its handles to resize it. The selected shape is red and unselected shapes are
white.
Left-drag any handle to resize, or right-drag any handle to rotate around the shape's center.
Right-click a shape to save, clone, or delete it. Right-click outside all shapes to save or delete
all ROIs. Use the toolbar actions or **Ctrl+Z** and **Ctrl+Shift+Z** to undo and redo shape creation,
movement, resizing, rotation, cloning, and deletion. Rotating the source image starts a new shape
history. **Batch Save** applies the clicked shape from the current image to every open image;
**Batch Save All** applies only the current image's shapes. Shapes belonging to other images never
participate. The batch table marks an image invalid when any requested ROI extends beyond its
bounds, and valid PNG names include one operation timestamp plus a sequence number. Pixels outside
non-rectangular shapes are transparent.

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
