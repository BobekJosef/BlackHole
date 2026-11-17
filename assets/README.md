# Sky maps

Background images for the *Image map* background. Everything here except this file is git-ignored:
maps are large, so they are downloaded, not committed.

```sh
scripts/download_sky_maps.sh   # fetches the maps below into this folder
```

The panel lists every map in this folder (*Rescan* after adding one). A map is either

- an **equirectangular** image, width = 2 × height: Radiance `.hdr` (HDR, best: stars keep their
  brightness ratios), PNG (8 or 16 bit), JPEG, TGA, BMP; or
- a **cube map**: a folder with six square faces named `px nx py ny pz nz`, `posx negx …`, or
  `right left top bottom front back` (any of the formats above), authored y-up as usual for OpenGL.

Other paths can be typed into the panel's *Path* field.

| File | Source | Licence |
|------|--------|---------|
| `eso0932a.jpg` (6000 × 3000, 8-bit) | ESO, *The Milky Way panorama*, <https://www.eso.org/public/images/eso0932a/> — galactic coordinates, the galactic centre in the middle | CC BY 4.0, credit **ESO/S. Brunier** |

More: NASA SVS *Deep Star Maps 2020* (<https://svs.gsfc.nasa.gov/4851>, HDR star maps; the EXR files need
converting to `.hdr`, as EXR is not read yet), or any HDRI sky from Poly Haven (CC0).
