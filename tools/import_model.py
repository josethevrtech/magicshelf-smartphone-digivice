"""Run in Blender 5.2+ to import the user's original Courage phone faithfully."""
import bpy
import numpy as np
import struct
import json
import argparse
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--source', required=True, type=Path, help='Directory containing Smartphone Digivice.blend')
parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1]/'assets')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
SOURCE = args.source
DEST = args.output
DEST.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(SOURCE/"Smartphone Digivice.blend"),
                        load_ui=False, use_scripts=False)

# Single-crest Adventure case and Taichi's matching orange button illumination.
phone = bpy.data.objects["Phone"]
case = bpy.data.objects["Case1"]
case.data.materials[0] = bpy.data.materials["COLOR Taichi"]
materials = ["COLOR Taichi", "MatteBlack", "ShineWhite", "ShineBlack LIGHTS",
             "ShineSilver", "Screen"]
SCALE = .055

def pixels(name):
    image = bpy.data.images[name]
    if max(image.size)>1024:
        image = image.copy()
        image.scale(1024, 1024)
    array = np.empty(image.size[0]*image.size[1]*4, dtype=np.float32)
    image.pixels.foreach_get(array)
    return array.reshape(image.size[1],image.size[0],4)

def write_texture(index, rgb, emission):
    # Store sRGB color and an independent emission mask. A raw upload avoids
    # premultiplied-alpha PNG loaders destroying colors where emission is zero.
    rgb = np.clip(rgb,0,1)
    srgb = np.where(rgb<=.0031308,rgb*12.92,1.055*np.power(rgb,1/2.4)-.055)
    rgba = np.concatenate((srgb, emission[...,None]),axis=2)
    raw = np.round(np.clip(rgba,0,1)*255).astype(np.uint8)
    h,w,_ = raw.shape
    with (DEST/f"body-{index}.rgba").open("wb") as out:
        out.write(struct.pack("<II",w,h))
        out.write(raw.tobytes())
    return w,h

material_records = []
for index,name in enumerate(materials):
    mat = bpy.data.materials[name]
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    base = np.array(bsdf.inputs["Base Color"].default_value[:3],dtype=np.float32)
    rough = bsdf.inputs["Roughness"].default_value
    metallic = bsdf.inputs["Metallic"].default_value
    if name == "COLOR Taichi":
        mix = mat.node_tree.nodes["Mix"]
        assert mix.blend_type=="ADD"
        factor = mix.inputs[0].default_value
        base = np.array(mix.inputs[6].default_value[:3],dtype=np.float32)
        p = pixels("Crest1 courage.png")
        rgb = base + p[...,:3]*factor
        glow = np.zeros(p.shape[:2])
    elif name == "MatteBlack":
        p = pixels("matte black marks.png")
        rgb = base*(1-p[...,3,None])+p[...,:3]*p[...,3,None]
        glow = np.zeros(p.shape[:2])
    elif name == "ShineBlack LIGHTS":
        p = pixels("button lights.png")
        color = np.array(mat.node_tree.nodes["Color.008"].outputs[0].default_value[:3])
        glow = p[...,:3].mean(axis=2)
        rgb = base*(1-glow[...,None])+color*glow[...,None]
    else:
        # The original emissive white placeholder screen becomes dark glass.
        rgb = base.reshape(1,1,3)
        glow = np.zeros((1,1))
    dimensions = write_texture(index,rgb,glow)
    material_records.append(dict(name=name,roughness=rough,metallic=metallic,
                                 texture=f"body-{index}.rgba",dimensions=dimensions))

groups = [[] for _ in materials]
for obj in (phone,case):
    mesh = obj.data
    mesh.calc_loop_triangles()
    uv = mesh.uv_layers.active.data
    normals = mesh.corner_normals
    for triangle in mesh.loop_triangles:
        name = mesh.materials[triangle.material_index].name
        if name == "ScreenTransp":
            continue # optical clear cover above the earpiece, not a solid shell
        index = materials.index(name)
        for loop_index in triangle.loops:
            loop = mesh.loops[loop_index]
            p = mesh.vertices[loop.vertex_index].co*SCALE
            n = normals[loop_index].vector
            t = uv[loop_index].uv
            groups[index].append((*p,*n,*t))

with (DEST/"device.mesh").open("wb") as out:
    out.write(b"DPH2")
    out.write(struct.pack("<II",len(materials),sum(map(len,groups))))
    first=0
    for group,mat in zip(groups,material_records):
        out.write(struct.pack("<IIff",first,len(group),mat["roughness"],mat["metallic"]))
        first+=len(group)
    for group in groups:
        for v in group:
            out.write(struct.pack("<8f",*v))
info = dict(source=str(SOURCE/"Smartphone Digivice.blend"),author="Jojogape",
            case="Case1",material="COLOR Taichi",crest="Crest1 courage.png",
            scale=SCALE,triangles=sum(map(len,groups))//3,materials=material_records,
            screen=dict(half_height=1.5*SCALE,half_width=1.5*SCALE*480/1046,
                        center_y=.24*SCALE,z=.0635*SCALE),
            body=dict(half_width=1.205*SCALE,half_height=2.08*SCALE))
(DEST/"model-info.json").write_text(json.dumps(info,indent=2)+"\n")
print(json.dumps(info,indent=2))
