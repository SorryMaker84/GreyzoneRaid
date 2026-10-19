import unreal

unreal.log("R7_TRANSFORM_DOC " + str(unreal.Transform.__doc__))
unreal.log("R7_ROTATOR_METHODS " + repr([name for name in dir(unreal.Rotator) if "quat" in name]))
transform = unreal.Transform()
unreal.log("R7_TRANSFORM_PROPERTIES " + transform.export_text())
transform.set_editor_property("translation", unreal.Vector(10, 20, 30))
transform.set_editor_property("rotation", unreal.Rotator(yaw=45).quaternion())
transform.set_editor_property("scale3d", unreal.Vector(2, 3, 4))
unreal.log("R7_TRANSFORM_PROBE_OK " + transform.export_text())
