#pragma once


function void crack_packed_transform(const int op, prim; export vector4 rot; export vector scale)
{
    vector t, r;
    cracktransform(XFORM_SRT, XFORM_XYZ, {0, 0, 0}, getpackedtransform(op, prim), t, r, scale);
    rot = eulertoquaternion(radians(r), XFORM_XYZ);
}

function int get_asset_bounds(const string asset_ref; export vector bmin, bmax)  // return 1 if this asset has bounds info
{
    bmin = { 0.0, 0.0, 0.0 };
    bmax = { 0.0, 0.0, 0.0 };

    string asset_infos[] = split(asset_ref, ";");
    if (len(asset_infos) <= 1)
        return 0;
    
    for (int info_idx = len(asset_infos) - 1; info_idx >= 1; --info_idx)
    {
        if (startswith(asset_infos[info_idx], "bounds:"))  // contains one mesh
        {
            string bounds_str = asset_infos[info_idx];
            string bound_strs[] = split(bounds_str[7:], ",");
            bmin.x = atof(bound_strs[0]); bmin.y = atof(bound_strs[2]); bmin.z = atof(bound_strs[4]);
            bmax.x = atof(bound_strs[1]); bmax.y = atof(bound_strs[3]); bmax.z = atof(bound_strs[5]);
            return 1;
        }
    }

    dict container = json_loads(asset_infos[1], 2);
    string inst_refs[] = keys(container);
    int first = 1;
    foreach (string inst_ref; inst_refs)
    {
        dict inst_info = container[inst_ref];
        if (isvalidindex(inst_info, "bounds"))  // only cal mesh bounds
        {
            float bounds[] = inst_info["bounds"];
            vector bbox_corners[] = array(
                set(bounds[0], bounds[2], bounds[4]), set(bounds[1], bounds[2], bounds[4]),
                set(bounds[0], bounds[3], bounds[4]), set(bounds[1], bounds[3], bounds[4]),
                set(bounds[0], bounds[2], bounds[5]), set(bounds[1], bounds[2], bounds[5]),
                set(bounds[0], bounds[3], bounds[5]), set(bounds[1], bounds[3], bounds[5]));
            
            float P[] = inst_info["P"];
            float rot[] = inst_info["rot"];
            float scale[] = inst_info["scale"];
                
            int num_insts = len(P) / 3;
            for (int i = 0; i < num_insts; ++i)
            {
                matrix xform = maketransform(XFORM_SRT, XFORM_XYZ, set(P[i * 3], P[i * 3 + 1], P[i * 3 + 2]),
                    degrees(quaterniontoeuler(vector4(set(rot[i * 4], rot[i * 4 + 1], rot[i * 4 + 2], rot[i * 4 + 3])), XFORM_XYZ)),
                    set(scale[i * 3], scale[i * 3 + 1], scale[i * 3 + 2]));
                foreach (vector bbox_corner; bbox_corners)
                {
                    bbox_corner *= xform;
                    if (first)
                    {
                        bmin = bbox_corner;
                        bmax = bbox_corner;
                        first = 0;
                    }
                    else
                    {
                        if (bbox_corner.x < bmin.x) bmin.x = bbox_corner.x; else if (bbox_corner.x > bmax.x) bmax.x = bbox_corner.x;
                        if (bbox_corner.y < bmin.y) bmin.y = bbox_corner.y; else if (bbox_corner.y > bmax.y) bmax.y = bbox_corner.y;
                        if (bbox_corner.z < bmin.z) bmin.z = bbox_corner.z; else if (bbox_corner.z > bmax.z) bmax.z = bbox_corner.z;
                    }
                }
            }
        }
    }
        
    return first == 0;
}

function string get_op_cook_folder_suffix()
{
    string node_label = split(opfullpath("."), "/")[1];
    return "Cook/" + node_label[:-9] + "/" + node_label[-8:] + "/";
}


// -------- delta_info --------
function int get_transform_type(const string transformstr)  // return -1: not a matrix, 0: ident 1: translate, 2: rotate, 3: scale
{
    string transform_infos[] = split(transformstr, ";");
    
    if (len(transform_infos) == 4) {
        if (transform_infos[0] != "0.0,0.0,0.0")
            return 1;
        else if (transform_infos[1] != "0.0,0.0,0.0")
            return 2;
        else if (transform_infos[2] != "1.0,1.0,1.0")
            return 3;
        
        return 0;
    }
    
    return -1;
}

function matrix parse_transform(const string transformstr)
{
    string transform_infos[] = split(transformstr, ";"), tstrs[];
    vector t, r, s, p;
    
    tstrs = split(transform_infos[0], ",");  // translate
    t.x = atof(tstrs[0]); t.y = atof(tstrs[1]); t.z = atof(tstrs[2]);
    
    tstrs = split(transform_infos[1], ",");  // rotate
    r.x = atof(tstrs[0]); r.y = atof(tstrs[1]); r.z = atof(tstrs[2]);
    
    tstrs = split(transform_infos[2], ",");  // scale
    s.x = atof(tstrs[0]); s.y = atof(tstrs[1]); s.z = atof(tstrs[2]);
    
    tstrs = split(transform_infos[3], ",");  // pivot
    p.x = atof(tstrs[0]); p.y = atof(tstrs[1]); p.z = atof(tstrs[2]);
    
    return maketransform(XFORM_TRS, XFORM_XZY, t, r, s, p);
}

function vector4 parse_region(const string regionstr)
{
    vector4 region = { -1, -1, -1, -1 };
    string coordstrs[] = split(regionstr, "-");

    string valuestrs[] = split(coordstrs[0], ",");
    region.x = atoi(valuestrs[0]);
    region.y = atoi(valuestrs[1]);
    
    valuestrs = split(coordstrs[1], ",");
    region.z = atoi(valuestrs[0]);
    region.w = atoi(valuestrs[1]);

    return region;
}

function int parse_click_position(const string class_str; export vector click_pos)
{
    click_pos = { 0.0, 0.0, 0.0 };
    string infos[] = split(class_str, ":");
    if (len(infos) >= 2) {
        string pos_strs[] = split(infos[1], ",");
        click_pos.x = atof(pos_strs[0]);
        click_pos.y = atof(pos_strs[1]);
        click_pos.z = atof(pos_strs[2]);

        return 1;
    }
    
    return 0;  // does not have click pos recorded
}

function string[] parse_string_values(const string delta_info)
{
    int slash_idcs[] = find(delta_info, "/");
    return split(delta_info[slash_idcs[2] + 1:], "|");
}


// -------- client exclusively (unreal ver.) --------
function string remove_class_prefix(const string export_text_path)
{
    int split_idx = find(export_text_path, ";");  // maybe contains asset info, such as bounds
    string parts[] = split((split_idx >= 0) ? export_text_path[:split_idx] : export_text_path, "'");
    return ((len(parts) >= 2) ? parts[1] : ((split_idx >= 0) ? export_text_path[:split_idx] : export_text_path));
}

function string get_asset_name(const string export_text_path)
{
    string asset_path_infos[] = split(remove_class_prefix(export_text_path), ".");
    return ((len(asset_path_infos) >= 2) ? asset_path_infos[-1] : split(asset_path_infos[0], "/")[-1]);
}

function dict parse_preset(const string op; export string preset_name)
{
    dict parms;
    int num_parms = npoints(op);
    for (int pt = 0; pt < num_parms; ++pt) {
        string parm_name = point(op, "unreal_data_table_rowname", pt);
        int type = point(op, "unreal_data_table_Type", pt);
        int size = point(op, "unreal_data_table_Size", pt);
        if (type == 0 && size == 1)  // int
            parms[parm_name] = int(vector4(point(op, "unreal_data_table_NumericValues", pt)).x);
        else if (type <= 1) {  // int tuples or float
            vector4 values = point(op, "unreal_data_table_NumericValues", pt);
            if (size <= 1) parms[parm_name] = values.x;
            else if (size == 2) parms[parm_name] = set(values.x, values.y);
            else if (size == 3) parms[parm_name] = set(values.x, values.y, values.z);
            else parms[parm_name] = values;
        }
        else if (type == 2)  // string
            parms[parm_name] = string(point(op, "unreal_data_table_StringValue", pt));
        else if (type == 3) {  // object
            string object_path = point(op, "unreal_data_table_ObjectValue", pt);
            string asset_info = point(op, "unreal_data_table_StringValue", pt);
            if (strlen(asset_info))
                object_path += ";" + asset_info;
            parms[parm_name] = object_path;
        }
        else if (type == 4)  // the counts of multi parm are stores as string
            parms[parm_name] = atoi(string(point(op, "unreal_data_table_StringValue", pt)));
    }
    
    preset_name = get_asset_name(string(point(op, "unreal_object_path", 0)));
    
    return parms;
}
