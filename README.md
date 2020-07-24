# nif2fbx

nif2fbx is a library capable of converting 3D models from NIF format (used by
The Elder Scrolls series and many other series) to FBX format, which is
compatible with the most of modern authoring tools.

See nif2fbx-test for an usage example.

Please note that nif2fbx is incomplete and only processes a very limited set
of models correctly.

# Building

nif2fbx may be built using normal CMake procedures, and is generally
intended to included into an outer project as a submodule or by any other
means.

Additionally, nif2fbx requires a copy of Autodesk FBX SDK to be present.

Please note that nif2fbx uses git submodules, which should be retrieved
before building.

# Licensing

nif2fbx is licensed under the terms of the MIT license (see LICENSE).

Note that nif2fbx also references nifparse, which has rather particular
licensing conditions, and MIT-licensed jsoncpp, as submodules.
