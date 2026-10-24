"""Reproducible NASA Shuttle D -> OBJ/MTL adaptation, not a runtime dependency.

Run from the project root with numpy and DracoPy==2.2.0 available.
Inputs are the four original GLBs in assets/nasa-shuttle/source.
Embedded base-color images are extracted byte-for-byte; no image synthesis.
"""
import json
import struct
from pathlib import Path

import DracoPy
import numpy as np

DEST = Path(__file__).resolve().parents[1] / "assets/nasa-shuttle"
obj = ["# NASA/Johnson Space Center — Space Shuttle (D)", "mtllib shuttle.mtl"]
mtl = ["# Original NASA materials, adapted for the Stellarsue renderer"]
vertex_offset = 0
triangle_count = 0
image_files = set()


def convert(filename, placements):
    global vertex_offset, triangle_count
    blob = (DEST / "source" / filename).read_bytes()
    magic, version, size = struct.unpack_from("<III", blob)
    assert magic == 0x46546C67 and version == 2 and size == len(blob)
    length, kind = struct.unpack_from("<II", blob, 12)
    assert kind == 0x4E4F534A
    doc = json.loads(blob[20:20 + length])
    binary = blob[28 + length:]
    prefix = Path(filename).stem

    def view(index):
        v = doc["bufferViews"][index]
        offset = v.get("byteOffset", 0)
        return binary[offset:offset + v["byteLength"]]

    def accessor(index):
        a = doc["accessors"][index]
        v = doc["bufferViews"][a["bufferView"]]
        scalar = {5126: "f", 5123: "H", 5125: "I", 5121: "B"}[a["componentType"]]
        count = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
        stride = v.get("byteStride", struct.calcsize(scalar) * count)
        raw = view(a["bufferView"])
        return np.array([struct.unpack_from("<" + scalar * count, raw,
                         a.get("byteOffset", 0) + i * stride) for i in range(a["count"])])

    for i, material in enumerate(doc["materials"]):
        pbr = material.get("pbrMetallicRoughness", {})
        color = pbr.get("baseColorFactor", [1, 1, 1, 1])[:3]
        roughness = max(0.10, pbr.get("roughnessFactor", 0.47))
        metal = pbr.get("metallicFactor", 0)
        # Engine bells are metal, while the heat shield remains a rough ceramic.
        if prefix == "engine":
            metal, roughness = 0.65, 0.42
        mtl.extend([f"newmtl {prefix}_{i}", "Kd " + " ".join(f"{c:.6f}" for c in color),
                    f"Pr {roughness:.6f}", f"Pm {metal:.6f}"])
        if "baseColorTexture" in pbr:
            texture = doc["textures"][pbr["baseColorTexture"]["index"]]
            img = doc["images"][texture["source"]]
            if "bufferView" not in img:
                # NASA's separate door GLBs refer to JPGs not included in the
                # download. Retain their base color instead of inventing a map.
                print(f"{prefix}: external {img.get('uri')} unavailable; using base material")
                continue
            suffix = {"image/jpeg": ".jpg", "image/png": ".png"}[img["mimeType"]]
            name = f"{prefix}_{texture['source']}{suffix}"
            (DEST / name).write_bytes(view(img["bufferView"]))
            image_files.add(name)
            mtl.append(f"map_Kd {name}")

    def visit(index, parent):
        node = doc["nodes"][index]
        # These particular source files use matrices or identity nodes only.
        assert not any(k in node for k in ("translation", "rotation", "scale"))
        local = np.array(node.get("matrix", np.eye(4).flatten(order="F"))).reshape(4, 4, order="F")
        world = parent @ local
        if "mesh" in node:
            for primitive in doc["meshes"][node["mesh"]]["primitives"]:
                emit(primitive, world)
        for child in node.get("children", []):
            visit(child, world)

    def emit(primitive, world):
        global vertex_offset, triangle_count
        assert primitive.get("mode", 4) == 4
        draco = primitive.get("extensions", {}).get("KHR_draco_mesh_compression")
        if draco:
            mesh = DracoPy.decode(view(draco["bufferView"]))
            by_id = {a["unique_id"]: a["data"] for a in mesh.attributes}
            attributes = {key: by_id[value] for key, value in draco["attributes"].items()}
            faces = mesh.faces
        else:
            attributes = {key: accessor(value) for key, value in primitive["attributes"].items()}
            faces = accessor(primitive["indices"]).reshape(-1, 3)
        positions = attributes["POSITION"] @ world[:3, :3].T + world[:3, 3]
        normals = attributes["NORMAL"] @ np.linalg.inv(world[:3, :3])
        normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-12)
        uvs = attributes.get("TEXCOORD_0", np.zeros((len(positions), 2)))
        for translation in placements:
            # NASA model axes: +X nose, +Z up. Runtime: +Z nose, +Y up.
            points = (positions + np.array(translation))[:, [1, 2, 0]]
            directions = normals[:, [1, 2, 0]]
            obj.append(f"usemtl {prefix}_{primitive['material']}")
            obj.extend("v " + " ".join(f"{v:.6f}" for v in point) for point in points)
            obj.extend("vn " + " ".join(f"{v:.6f}" for v in normal) for normal in directions)
            obj.extend(f"vt {uv[0]:.7f} {1.0 - uv[1]:.7f}" for uv in uvs)
            for face in faces:
                indices = [int(i) + vertex_offset + 1 for i in face]
                obj.append("f " + " ".join(f"{i}/{i}/{i}" for i in indices))
            vertex_offset += len(points)
            triangle_count += len(faces)

    for index in doc["scenes"][doc.get("scene", 0)]["nodes"]:
        visit(index, np.eye(4))


convert("shuttle.glb", [(0, 0, 0)])
convert("door-port.glb", [(0, 0, 0)])
convert("door-starboard.glb", [(0, 0, 0)])
# Separate NASA engine part is provided around its own pivot. Assemble three
# bells at the orbiter's aft bulkhead; this placement is a visual approximation.
convert("engine.glb", [(-527, 0, 32), (-527, -62, -65), (-527, 62, -65)])
(DEST / "shuttle.obj").write_text("\n".join(obj) + "\n")
(DEST / "shuttle.mtl").write_text("\n".join(mtl) + "\n")
print(f"Prepared {triangle_count} triangles, {vertex_offset} vertices, {len(image_files)} textures")
print("Textures:", ", ".join(sorted(image_files)))
