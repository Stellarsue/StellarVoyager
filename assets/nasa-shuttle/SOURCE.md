# Space Shuttle (D) — model provenance

Original model credit: NASA/Johnson Space Center.

- Model page: https://science.nasa.gov/3d-resources/space-shuttle-d/
- Usage guidelines: https://www.nasa.gov/nasa-brand-center/images-and-media/
- Downloaded: 2026-10-24.
- Download base: `https://assets.science.nasa.gov/content/dam/science/cds/3d/resources/model/space-shuttle-(d)/`
- Original files: `Space Shuttle (D).glb`, `Space Shuttle (D) door-prt.glb`,
  `Space Shuttle (D) door-stb.glb`, `Space Shuttle (D) eng.glb`.
- Originals are preserved in `source/`, renamed to `shuttle.glb`,
  `door-port.glb`, `door-starboard.glb`, and `engine.glb` respectively.

This application is independent of NASA and does not imply NASA endorsement,
review or approval. The source credit applies to the original model, not to the
artistic black-hole scene, flight path or application rendering. The NASA name
and any original vehicle markings are not used as the application's branding.
This is not a CC0 asset; consult the linked NASA media and brand guidelines for
redistribution and any commercial or promotional use.

## Adaptation

`tools/prepare_shuttle.py` decodes the original Draco geometry and converts it to
OBJ/MTL with UVs, normals, base colors and roughness. Embedded JPEGs are extracted
unchanged. Run with Python, NumPy and `DracoPy==2.2.0`; these are development-only
dependencies, not runtime requirements.

The source has a +X nose and +Z up. Coordinates are rotated to +Z nose / +Y up for
Stellarsue. Separate door meshes use their supplied root transforms. Three
copies of the separate engine mesh are placed at the aft bulkhead; those offsets
are a visual approximation, not engineering measurements. Engine-bell materials
are given metallic shading. The door GLBs reference two external JPEGs absent
from the download; those surfaces use their base materials, not invented maps.

The assembled model has 37,004 triangles and 11 base-color textures. The runtime
normalizes its longest dimension, uploads the textures as 512px mipmapped sRGB
array layers, and lights it with a roughness/metalness GGX approximation. It is
not a photorealistic or physically complete spacecraft simulation.

## Original SHA-256

```text
d0015681ff092d36062fdfed78e2bd1ca0f86e90862840d25bae11c686ce3549  source/shuttle.glb
cd8e84f3b108137f573ef260b5be8c35816fbb89e273b4648b20e35aee1ed419  source/door-port.glb
f48cf6e5f11ec13f28b143526cdd27751a5fe7117b45472b5d49b185c9ac45dc  source/door-starboard.glb
e31cfb57e3365f036d40cee79ac9e2f8b5f4c48849f6aa9e19aee4075846a0bc  source/engine.glb
```
