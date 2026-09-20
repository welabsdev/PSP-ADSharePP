# ADShare source architecture

The source tree is divided by responsibility instead of by screen size or code
order. The goal is to make future features easier to add without turning the
project back into a single large translation unit.

## Dependency direction

```text
main.cpp
   |
   v
app.cpp
   |-------------------------------|
   v               v               v
screens.cpp     browser.cpp      transfer.cpp
   |               |               |
   v               |               v
ui.cpp             |         data_protocol.cpp
   |               |               |
   |---------------|---------------|
                   v
                adhoc.cpp
                   |
                   v
              hardware.cpp

All modules share small POD models and runtime declarations from adshare.hpp.
```

`adshare.hpp` intentionally contains the packed network structures because both
control and data layers must agree on their exact memory layout.

## Why C++ here?

The refactor uses C++ where it provides concrete value on PSP without requiring
heavy STL usage:

- namespaces for symbol isolation;
- `Application` for deterministic application lifetime;
- RAII for transfer cleanup;
- `constexpr` for immutable configuration;
- `static_assert` for the 9-byte Ad Hoc product ID requirement;
- deleted copy operations for ownership-style classes.

The networking and file-transfer buffers remain fixed-size PSP-friendly data
structures, avoiding unnecessary heap allocation in the hot transfer path.
