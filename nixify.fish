#!/usr/bin/env fish

# NOTE: This works for TowBoot. Grub requires custom mkrescue paths...

echo "Patching Shebangs"
# sed -i "s/#!\/bin\/bash/#!\/usr\/bin\/env\ bash/g" ./**.sh
sd '#!/bin/bash' '#!/usr/bin/env bash' ./**.sh

echo "Patching /bin Paths"
# sed -i "s/\ \/bin\//\ \/run\/current-system\/sw\/bin\//g" ./cmake/**.txt
sd ' /bin/' ' /run/current-system/sw/bin/' ./cmake/**.txt
