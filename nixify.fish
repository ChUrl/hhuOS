#!/usr/bin/env fish

# NOTE: This is run through fish to enable the ./**.sh globbing

echo "Patching Shebangs"
# sed -i "s/#!\/bin\/bash/#!\/usr\/bin\/env\ bash/g" ./**.sh
# sd '#!/bin/bash' '#!/usr/bin/env bash' ./**.sh
git apply ./nixify_shebangs.patch

echo "Patching /bin Paths"
# sed -i "s/\ \/bin\//\ \/run\/current-system\/sw\/bin\//g" ./cmake/**.txt
# sd ' /bin/' ' /run/current-system/sw/bin/' ./cmake/**.txt
git apply ./nixify_binpaths.patch

echo "Patching Grub"
git apply ./nixify_grub.patch
