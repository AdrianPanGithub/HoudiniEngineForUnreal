import hou
import json


def get_bbox_geo(bounds):
    bmin, bmax = (bounds[0], bounds[2], bounds[4], ), (bounds[1], bounds[3], bounds[5], )
    geo_bbox = hou.Geometry()
    geo_bbox.createPoints([bmin, (bmin[0], bmax[1], bmin[2]), (bmax[0], bmax[1], bmin[2]), (bmax[0], bmin[1], bmin[2]),
        (bmin[0], bmin[1], bmax[2]), (bmin[0], bmax[1], bmax[2]), bmax, (bmax[0], bmin[1], bmax[2]), ])
    geo_bbox.createPolygons(((3, 2, 1, 0), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7), (4, 5, 6, 7), ))
    return geo_bbox

def append_asset_with_info(geo, attrib_inst, asset_info: str):
    if len(asset_info) <= 0:
        return None

    asset_infos = asset_info.split(";")
    if len(asset_infos) <= 1:
        return None

    for sub_info in asset_infos[1:]:
        if sub_info.startswith("bounds:"):  # contains one mesh
            bounds = [float(bound_str) for bound_str in sub_info[7:].split(",")]
            prim_packed = geo.createPackedGeometry(get_bbox_geo(bounds))
            prim_packed.points()[0].setAttribValue(attrib_inst, asset_infos[0])
            return prim_packed
        
    
    # is a container
    container = json.loads(asset_infos[1])
    geo_container = hou.Geometry()
    attrib_inst_container = geo_container.addAttrib(hou.attribType.Point, attrib_inst.name(), "", False, False)
    positions, insts, rot, scale = [], [], [], []
    for inst_ref in container:
        inst_info = container[inst_ref]
        
        if "bounds" in inst_info:  # is a mesh
            geo_packed = get_bbox_geo(inst_info["bounds"])
        else:  # is a decal, light, or something else
            geo_packed = hou.Geometry()
            geo_packed.createPoint()  # TODO: support some property input?
        geo_container.createPackedGeometry(geo_packed).points()[0].setAttribValue(attrib_inst_container, inst_ref)
        
        P_data, rot_data, scale_data = inst_info["P"], inst_info["rot"], inst_info["scale"]
        for i in range(int(len(P_data) / 3)):
            positions.append((P_data[i * 3], P_data[i * 3 + 1], P_data[i * 3 + 2]))
            insts.append(inst_ref)
        rot.extend(rot_data)
        scale.extend(scale_data)
    
    geo_insts = hou.Geometry()
    attrib_inst_insts = geo_insts.addAttrib(hou.attribType.Point, attrib_inst.name(), "", False, False)
    attrib_rot = geo_insts.addAttrib(hou.attribType.Point, "rot", hou.Vector4(), False, False)
    attrib_scale = geo_insts.addAttrib(hou.attribType.Point, "scale", hou.Vector3(), False, False)
    geo_insts.createPoints(positions)
    geo_insts.setPointStringAttribValues(attrib_inst_insts.name(), insts)
    geo_insts.setPointFloatAttribValues(attrib_rot.name(), rot)
    geo_insts.setPointFloatAttribValues(attrib_scale.name(), scale)
    
    geo_inst = hou.Geometry()
    
    verb_copy_to_points = hou.sopNodeTypeCategory().nodeVerb("copytopoints::2.0")
    verb_copy_to_points.setParms({ "useidattrib": 1, "idattrib": attrib_inst.name(), })
    verb_copy_to_points.execute(geo_inst, [geo_container, geo_insts])

    prim_packed = geo.createPackedGeometry(geo_inst)
    prim_packed.points()[0].setAttribValue(attrib_inst, asset_infos[0])
    return prim_packed
