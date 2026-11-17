#!/usr/bin/env sh
# Downloads the sky maps listed in assets/README.md into assets/ (skips files already there).
set -eu

assets="$(cd "$(dirname "$0")/.." && pwd)/assets"
mkdir -p "$assets"

download() { # url file
    if [ -f "$assets/$2" ]; then
        echo "have   $2"
        return
    fi
    echo "fetch  $2"
    curl --fail --location --progress-bar --output "$assets/$2.part" "$1"
    mv "$assets/$2.part" "$assets/$2"
}

# ESO/S. Brunier, CC BY 4.0: the Milky Way panorama, 6000 x 3000.
download https://cdn.eso.org/images/large/eso0932a.jpg eso0932a.jpg
