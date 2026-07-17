{
  lib,
  stdenv,
  cmake,
  ninja,
  qt6,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "quickshot";
  version = "2.0.0";

  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../CMakeLists.txt
      ../assets
      ../include
      ../src
      ../tests
    ];
  };

  nativeBuildInputs = [
    cmake
    ninja
    qt6.wrapQtAppsHook
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtsvg
  ];

  cmakeFlags = [
    (lib.cmakeBool "BUILD_TESTING" finalAttrs.finalPackage.doCheck)
  ];

  doCheck = true;

  meta = {
    description = "A C++20 and Qt 6 image ROI editor";
    mainProgram = "quickshot";
    platforms = lib.platforms.unix;
  };
})
