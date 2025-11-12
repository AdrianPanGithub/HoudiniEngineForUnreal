import hou


for obj_node in hou.node("/obj").children():
    if obj_node.type().name() == "geo":
        for sop_node in obj_node.children():
            type_name = sop_node.type().name()
            if (type_name == "sharedmemory_volumeinput" or type_name == "sharedmemory_geometryinput") and not sop_node.isHardLocked():
                sop_node.setHardLocked(True)
