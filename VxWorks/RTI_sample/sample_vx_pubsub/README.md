RTI VxWorks sample (typed pub/sub)

This folder contains a small RTI Connext publisher/subscriber sample configured to be cross-compiled with the repository's VxWorks toolchain.

Files:

- `CMakeLists.txt`  -- configures rtiddsgen, builds `rti_pub` and `rti_sub` (typed)
- `HelloWorld.idl`  -- IDL used for rtiddsgen to generate type support
- `src/publisher.cpp` -- typed publisher example
- `src/subscriber.cpp` -- typed subscriber example
- `generated/` (filled at configure time) -- generated type support from rtiddsgen

Quick build (Powershell, from repository root):

1) Ensure environment variables required by the toolchain are set (example values):
   $env:WIND_BASE = 'D:/WindRiver/vxworks/23.03'
   $env:NDDSHOME_CTL = 'D:/WindRiver/workspace/rti_connext_dds-7.3.0'
   $env:WIND_CC_SYSROOT = 'D:/WindRiver/vxworks/23.03/ppce6500/vsb'

2) Create an out-of-source build directory and run CMake with the toolchain file:
   mkdir build-vx; cd build-vx
   cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=..\cmake\toolchain-vxworks-ppce6500.cmake -DNDDSHOME_CTL="$env:NDDSHOME_CTL" ..\cmake\RTI_sample\sample_vx_pubsub

3) Build:
   ninja

Notes and checklist:

- The CMakeLists will attempt to run `rtiddsgen` from `${NDDSHOME_CTL}/bin` during configure to generate C++11 type support for `HelloWorld.idl` into the build folder.
- Ensure `rtiddsgen` is available on the host (Windows) and that `${NDDSHOME_CTL}` points to your RTI installation.
- If the RTI library filenames are different for your install, update `target_link_libraries` in `CMakeLists.txt` to the correct library names.

IDL / type generation notes

- Configure runs `rtiddsgen -language C++11 -d <build>/generated HelloWorld.idl`.
- CMake then compiles generated sources together with `publisher.cpp` and `subscriber.cpp` and links against RTI libraries.

If rtiddsgen is not available or you prefer to run it manually, run this from the sample source dir (host):

```powershell
& "$env:NDDSHOME_CTL\bin\rtiddsgen" -language C++11 -d build\generated HelloWorld.idl
```

Then configure CMake pointing at the generated folder (it will already be picked up by the provided CMakeLists if generated into the build dir).
