"""Author Frontier using installed shapes and the project's existing local art.

Run in the freshly built UE editor, under the root agent's process coordination.
No imports, edits, or saves are performed by importing this module. Depot is never
loaded or changed. Geometry is centimetres; +X east, +Y north. Route coordinates
describe walking surfaces, while preview coordinates describe the player centre.
"""
import json
import math
import sys
from collections import Counter
from pathlib import Path

MAP = "/Game/Maps/Frontier"
TAG = "FrontierGenerated"
ZONES = [
    {"id": "support", "name": "后勤检查站", "bounds_xy": [-8000, -7000, -3000, 1000]},
    {"id": "medical", "name": "营房医疗区", "bounds_xy": [-8000, 1000, 0, 7000]},
    {"id": "warehouse", "name": "中央仓储区", "bounds_xy": [-3000, -4200, 3000, 3000]},
    {"id": "fuel", "name": "燃料与排水区", "bounds_xy": [3000, -7000, 8000, 3000]},
    {"id": "comms", "name": "通讯台地区", "bounds_xy": [0, 3000, 8000, 7000]},
]
SPAWN = (-7000, -6100, 100)
MISSION = (2200, 4900, 700)
EXTRACTIONS = [
    {"id": "S-01", "name": "南侧公路撤离", "location_cm": [6900, -6000, 10], "required_mission_id": ""},
    {"id": "N-02", "name": "北侧铁路撤离", "location_cm": [6500, 5900, 310], "required_mission_id": "FrontierIntel"},
]

# Each seed is deliberate. Only four existing catalog IDs are used; no new art,
# inventory system, or random filler is needed to create a risk/value gradient.
LOOT = [
    ("S01", "support", (-6600, -4900, 48), "Medkit", "野战急救包", 75),
    ("S02", "support", (-4700, -2700, 143), "Ammo", "车间备用弹药", 35),
    ("S03", "support", (-5350, -1650, 48), "Tech", "维修电路板", 90),
    ("S04", "support", (-6600, -700, 48), "Rations", "后勤压缩口粮", 40),
    ("M01", "medical", (-6500, 3000, 143), "Medkit", "医疗站急救包", 75),
    ("M02", "medical", (-5000, 3200, 48), "Medkit", "备用医疗物资", 60),
    ("M03", "medical", (-4300, 4900, 48), "Ammo", "营房弹药包", 35),
    ("M04", "medical", (-3100, 4500, 143), "Tech", "巡逻记录设备", 140),
    ("W01", "warehouse", (-1700, -700, 48), "Ammo", "仓库制式弹药", 35),
    ("W02", "warehouse", (-900, 350, 143), "Tech", "加密储存模块", 260),
    ("W03", "warehouse", (500, -450, 48), "Rations", "应急口粮", 40),
    ("W04", "warehouse", (-1200, 1400, 448), "Tech", "军用光学组件", 360),
    ("W05", "warehouse", (450, 1400, 448), "Tech", "航电控制模块", 320),
    ("W06", "warehouse", (2300, -2200, 48), "Medkit", "装卸区急救包", 75),
    ("F01", "fuel", (4200, -3200, 48), "Ammo", "泵房备用弹药", 35),
    ("F02", "fuel", (5350, -2800, 143), "Tech", "压力控制仪", 190),
    ("F03", "fuel", (6550, 800, 48), "Tech", "燃料监测器", 230),
    ("F04", "fuel", (3900, 1400, 48), "Medkit", "维修站医疗包", 75),
    ("F05", "fuel", (5700, -4550, -232), "Tech", "遗落的通讯配件", 170),
    ("C01", "comms", (1300, 4150, 348), "Ammo", "通讯站备用弹药", 35),
    ("C02", "comms", (2650, 4050, 348), "Medkit", "通讯站急救包", 75),
    ("C03", "comms", (1450, 5050, 748), "Tech", "加密通讯核心", 420),
    ("C04", "comms", (4200, 4600, 348), "Tech", "信号处理模块", 280),
    ("C05", "comms", (6000, 5200, 348), "Rations", "撤离应急口粮", 40),
]

GUARDS = [
    ("SupportPatrol", "support", (-4200, -1000, 96), [(-4200, -1000, 95), (-3600, -1300, 95), (-3500, -2600, 95)], 1150, 750),
    ("MedicalPatrol", "medical", (-4100, 3200, 96), [(-4500, 3200, 95), (-3800, 3200, 95), (-3300, 3600, 95)], 1300, 800),
    ("WarehouseWest", "warehouse", (-1650, -200, 96), [(-1650, -200, 95), (-1650, 650, 95), (-1000, 650, 95)], 1750, 1050),
    ("WarehouseEast", "warehouse", (700, 400, 96), [(700, 400, 95), (100, 400, 95), (100, -250, 95)], 1700, 1050),
    ("WarehouseMezzanine", "warehouse", (-700, 1420, 496), [(-700, 1420, 495)], 1650, 1050),
    ("LoadingPatrol", "warehouse", (2600, -900, 96), [(2600, -900, 95), (2700, 350, 95), (2400, 1700, 95)], 1700, 1050),
    ("PumpPatrol", "fuel", (3400, -2200, 96), [(3400, -2200, 95), (3500, -3200, 95), (3300, -3800, 95)], 1600, 1000),
    ("FuelSouth", "fuel", (6700, -2200, 96), [(6700, -2200, 95), (7200, -1800, 95), (7200, -700, 95)], 1650, 1050),
    ("FuelNorth", "fuel", (4800, 1500, 96), [(4800, 1500, 95), (4200, 1800, 95), (3800, 1500, 95)], 1650, 1050),
    ("CommsApproach", "comms", (3500, 3900, 396), [(3500, 3900, 395), (3500, 3700, 395), (4700, 3700, 395)], 1750, 1100),
    ("CommsUpper", "comms", (2670, 5000, 796), [(2670, 5000, 795)], 1450, 950),
    ("RailPatrol", "comms", (5750, 4300, 396), [(5750, 4300, 395), (6400, 4300, 395), (6850, 5000, 395)], 1800, 1100),
]

ROUTES = [
    {"id": "insertion_warehouse", "name": "后勤至仓库主路", "waypoints_cm": [
        [-7000, -6100, 0], [-7000, -5500, 0], [-5700, -5500, 0], [-5700, -3600, 0], [-3000, -3500, 0],
        [-3000, -2200, 0], [-1000, -2200, 0], [-1000, -1200, 0], [-1000, -400, 0]]},
    {"id": "warehouse_mezzanine", "name": "仓库夹层", "waypoints_cm": [
        [-1000, -400, 0], [-1000, -1050, 0], [100, -1050, 0], [1200, -300, 0], [1200, 0, 0],
        [1200, 500, 200], [1200, 1000, 400], [800, 1350, 400], [-700, 1350, 400]]},
    {"id": "warehouse_comms", "name": "仓库至通讯主路", "waypoints_cm": [
        [700, 400, 0], [700, -350, 0], [1550, -350, 0], [1550, 400, 0], [1900, 400, 0], [2700, 2400, 0], [4200, 2550, 0],
        [4200, 2600, 0], [4200, 3100, 150], [4200, 3600, 300], [3400, 3600, 300], [3400, 3450, 300], [1800, 3450, 300], [1800, 3750, 300]]},
    {"id": "comms_upper", "name": "通讯上层终端", "waypoints_cm": [
        [1800, 3750, 300], [1550, 3950, 300], [1100, 3900, 300], [1100, 4300, 500],
        [1100, 4700, 700], [1450, 4850, 700], [1750, 4900, 700], [1980, 4900, 700]]},
    {"id": "western_perimeter", "name": "外围医疗搜刮线", "waypoints_cm": [
        [-7400, -4200, 0], [-7400, 1400, 0], [-5900, 1500, 0], [-5900, 2100, 0],
        [-6200, 2650, 0], [-6050, 2800, 0], [-5650, 2800, 0], [-5200, 2800, 0], [-4500, 2800, 0], [-3900, 3300, 0], [-2200, 3300, 0],
        [-1300, 4400, 0], [-1000, 4400, 0], [-500, 4400, 150], [0, 4400, 300], [400, 4400, 300]]},
    {"id": "southern_extraction", "name": "燃料站至南侧撤离", "waypoints_cm": [
        [1900, 400, 0], [3200, -1400, 0], [3500, -2100, 0], [6100, -2150, 0],
        [6800, -3500, 0], [7600, -3900, 0], [7600, -5500, 0], [6900, -6000, 0]]},
    {"id": "drainage", "name": "下沉排水替代路线", "waypoints_cm": [
        [1000, -4550, 0], [1500, -4550, -140], [2000, -4550, -280],
        [3800, -4550, -280], [4500, -4550, -280], [5700, -4550, -280],
        [6200, -4550, -280], [6700, -4550, -140], [7200, -4550, 0], [7600, -5100, 0], [6900, -6000, 0]]},
    {"id": "northern_extraction", "name": "通讯至北侧撤离", "waypoints_cm": [
        [3150, 4500, 300], [3650, 5300, 300], [5000, 5700, 300], [5700, 5900, 300], [6500, 5900, 300]]},
]

PREVIEW_VIEWS = [
    {"id": "01_insertion", "location_cm": [-7000, -6100, 88], "yaw": 45, "pitch": 0},
    {"id": "02_warehouse_approach", "location_cm": [-1800, -2400, 88], "yaw": 62, "pitch": 0},
    {"id": "03_warehouse_inside", "location_cm": [-1050, -1050, 88], "yaw": 48, "pitch": 5},
    {"id": "04_warehouse_mezzanine", "location_cm": [600, 1110, 488], "yaw": -122, "pitch": -12},
    {"id": "05_fuel_drainage", "location_cm": [3650, -4550, -192], "yaw": 0, "pitch": 4},
    {"id": "06_comms_approach", "location_cm": [5000, 3750, 388], "yaw": 144, "pitch": 5},
    {"id": "07_intel_terminal", "location_cm": [1980, 4900, 788], "yaw": 0, "pitch": -8},
    {"id": "08_tactical_map", "location_cm": [-7000, -6100, 88], "yaw": 45, "pitch": 0, "show_tactical_map": True},
]


class FrontierBuilder:
    def __init__(self, unreal, materials):
        self.u = unreal
        self.assets = unreal.EditorAssetLibrary
        self.actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.materials = materials
        self.meshes = {name: unreal.load_asset("/Engine/BasicShapes/" + name + "." + name)
                       for name in ("Cube", "Cylinder", "Sphere", "Cone")}
        assert all(self.meshes.values()), "Installed Engine BasicShapes required"
        self.font = unreal.load_asset("/Game/UI/Fonts/F_RaidChineseWorld")
        assert self.font, "The project's existing Chinese world font is required"
        self.materials["fence"] = unreal.load_asset("/Game/Materials/M_ChainLink")
        assert self.materials["fence"], "Existing chain link material required"
        self.clusters = {}
        self.counts = Counter()
        self.report = {"schema_version": 1, "map": MAP, "units": "cm", "north_axis": "+Y",
                       "bounds_cm": {"min": [-8000, -7000, -280], "max": [8000, 7000, 1800]},
                       "spawn": {"location_cm": list(SPAWN), "yaw": 35}, "zones": ZONES,
                       "buildings": [], "ramps": [], "routes": ROUTES, "loot": [], "guards": [],
                       "extractions": [], "preview_views": PREVIEW_VIEWS,
                       "route_coordinate_convention": "walkable surface; player centre adds capsule half-height 88cm"}

    def tag(self, actor, ident, zone="environment", kind="geometry"):
        assert actor, "Could not spawn " + ident
        actor.set_actor_label(ident)
        actor.set_editor_property("tags", [self.u.Name(TAG), self.u.Name("R7Id:" + ident), self.u.Name("R7Zone:" + zone)])
        actor.set_folder_path(self.u.Name("Frontier/" + zone + "/" + kind))
        self.counts[kind + "_actors"] += 1
        return actor

    def rotation(self, yaw=0, pitch=0, roll=0):
        return self.u.Rotator(pitch=pitch, yaw=yaw, roll=roll)

    def shape(self, ident, xyz, size, mat="concrete", mesh="Cube", yaw=0, pitch=0, roll=0,
              solid=False, shadow=True, instance=None, zone="environment"):
        assert min(size) > 0, (ident, size)
        use_instances = not solid if instance is None else instance
        rotation = self.rotation(yaw, pitch, roll)
        if use_instances:
            key = (zone, mesh, mat, solid, shadow)
            if key not in self.clusters:
                cluster = self.tag(self.actors.spawn_actor_from_class(self.u.RaidPropCluster, self.u.Vector()),
                                   "Cluster_" + "_".join(map(str, key)), zone, "clusters")
                cluster.configure_cluster(self.meshes[mesh], self.materials[mat], solid, shadow)
                self.clusters[key] = cluster
            transform = self.u.Transform()
            transform.set_editor_property("translation", self.u.Vector(*xyz))
            transform.set_editor_property("rotation", rotation.quaternion())
            transform.set_editor_property("scale3d", self.u.Vector(*(n / 100.0 for n in size)))
            self.clusters[key].add_world_instance(transform)
            self.counts["instances"] += 1
            return None
        actor = self.tag(self.actors.spawn_actor_from_class(self.u.StaticMeshActor, self.u.Vector(*xyz), rotation),
                         ident, zone, "solids" if solid else "details")
        comp = actor.static_mesh_component
        comp.set_static_mesh(self.meshes[mesh])
        comp.set_material(0, self.materials[mat])
        comp.set_mobility(self.u.ComponentMobility.STATIC)
        comp.set_collision_profile_name("BlockAll" if solid else "NoCollision")
        comp.set_editor_property("can_ever_affect_navigation", solid)
        comp.set_cast_shadow(shadow)
        actor.set_actor_scale3d(self.u.Vector(*(n / 100.0 for n in size)))
        return actor

    def box(self, ident, xyz, size, mat="concrete", **kwargs):
        return self.shape(ident, xyz, size, mat, **kwargs)

    def cylinder(self, ident, xyz, diameter, height, mat="steel", **kwargs):
        return self.shape(ident, xyz, (diameter, diameter, height), mat, mesh="Cylinder", **kwargs)

    def beam(self, ident, a, b, thickness, mat="steel", **kwargs):
        delta = [b[i] - a[i] for i in range(3)]
        length = math.dist(a, b)
        return self.box(ident, tuple((a[i] + b[i]) / 2 for i in range(3)), (length, thickness, thickness), mat,
                        yaw=math.degrees(math.atan2(delta[1], delta[0])),
                        pitch=math.degrees(math.atan2(delta[2], math.hypot(*delta[:2]))), **kwargs)

    def pipe(self, ident, a, b, diameter=22, mat="steel", **kwargs):
        delta = [b[i] - a[i] for i in range(3)]
        return self.cylinder(ident, tuple((a[i] + b[i]) / 2 for i in range(3)), diameter, math.dist(a, b), mat,
                             yaw=math.degrees(math.atan2(delta[1], delta[0])),
                             pitch=math.degrees(math.atan2(delta[2], math.hypot(*delta[:2]))) - 90, **kwargs)

    def sign(self, ident, text, xyz, yaw=-90, size=42, color=(226, 231, 215, 255), zone="environment"):
        actor = self.tag(self.actors.spawn_actor_from_class(self.u.TextRenderActor, self.u.Vector(*xyz), self.rotation(yaw)),
                         ident, zone, "signs")
        component = actor.get_component_by_class(self.u.TextRenderComponent)
        component.set_font(self.font)
        component.set_text(text)
        component.set_world_size(size)
        component.set_text_render_color(self.u.Color(*color))
        component.set_horizontal_alignment(self.u.HorizTextAligment.EHTA_CENTER)
        component.set_collision_enabled(self.u.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("can_ever_affect_navigation", False)
        return actor

    def lamp(self, ident, xyz, zone, color=(1.0, .79, .52, 1), radius=1300, intensity=1800):
        self.box(ident + "_Housing", xyz, (85, 45, 16), "dark", zone=zone)
        self.box(ident + "_Lens", (xyz[0], xyz[1], xyz[2] - 10), (64, 30, 4), "warm_lamp", shadow=False, zone=zone)
        light = self.tag(self.actors.spawn_actor_from_class(self.u.PointLight, self.u.Vector(xyz[0], xyz[1], xyz[2] - 35)),
                         ident, zone, "lights")
        component = light.light_component
        component.set_mobility(self.u.ComponentMobility.MOVABLE)
        # This project's fixed exposure is tuned to a 3.1-intensity sun. The
        # original photometric lamp values clipped room surfaces to white.
        component.set_intensity(intensity * 0.025)
        component.set_light_color(self.u.LinearColor(*color))
        component.set_editor_property("attenuation_radius", radius)
        component.set_editor_property("cast_shadows", False)

    def wall(self, ident, centre, length, height, thickness, axis, openings, mat, zone):
        """Real wall segments and headers: no full collision box behind a doorway."""
        x, y, floor = centre
        cursor = -length / 2
        for index, (offset, width, clear_height) in enumerate(sorted(openings)):
            left, right = offset - width / 2, offset + width / 2
            assert cursor <= left and right <= length / 2 and 0 < clear_height <= height
            if left > cursor:
                along = (cursor + left) / 2
                position = (x + along if axis == "x" else x, y if axis == "x" else y + along, floor + height / 2)
                size = (left - cursor, thickness, height) if axis == "x" else (thickness, left - cursor, height)
                self.box(ident + "_Pier" + str(index), position, size, mat, solid=True, zone=zone)
            if clear_height < height:
                position = (x + offset if axis == "x" else x, y if axis == "x" else y + offset,
                            floor + (height + clear_height) / 2)
                size = (width, thickness, height - clear_height) if axis == "x" else (thickness, width, height - clear_height)
                self.box(ident + "_Header" + str(index), position, size, mat, solid=True, zone=zone)
            cursor = right
        if cursor < length / 2:
            along = (cursor + length / 2) / 2
            position = (x + along if axis == "x" else x, y if axis == "x" else y + along, floor + height / 2)
            size = (length / 2 - cursor, thickness, height) if axis == "x" else (thickness, length / 2 - cursor, height)
            self.box(ident + "_PierEnd", position, size, mat, solid=True, zone=zone)

    def building(self, ident, centre, width, depth, height, zone, doors, floor=0, mat="plaster", roof_rise=140):
        x, y = centre
        sides = {"south": ((x, y - depth / 2, floor), width, "x"),
                 "north": ((x, y + depth / 2, floor), width, "x"),
                 "west": ((x - width / 2, y, floor), depth, "y"),
                 "east": ((x + width / 2, y, floor), depth, "y")}
        data = {"id": ident, "zone": zone, "bounds_xy": [x - width / 2, y - depth / 2, x + width / 2, y + depth / 2],
                "floor_heights_cm": [floor], "doors": []}
        self.box(ident + "_Floor", (x, y, floor - 18), (width, depth, 36), "light_concrete", solid=True, zone=zone)
        for side, (origin, length, axis) in sides.items():
            openings = doors.get(side, [])
            self.wall(ident + "_" + side, origin, length, height, 28, axis, openings, mat, zone)
            for number, (offset, clear_width, clear_height) in enumerate(openings):
                door_position = [origin[0] + offset if axis == "x" else origin[0],
                                 origin[1] if axis == "x" else origin[1] + offset, floor]
                data["doors"].append({"side": side, "location_cm": door_position, "width_cm": clear_width,
                                      "height_cm": clear_height})
                for edge in (-1, 1):
                    px = door_position[0] + edge * (clear_width / 2 + 10) if axis == "x" else door_position[0]
                    py = door_position[1] if axis == "x" else door_position[1] + edge * (clear_width / 2 + 10)
                    self.box(ident + "_DoorFrame", (px, py, floor + clear_height / 2), (30, 38, clear_height + 20), "dark", zone=zone)
                self.box(ident + "_Threshold", (door_position[0], door_position[1], floor + 1),
                         (clear_width, 40, 2) if axis == "x" else (40, clear_width, 2), "hazard", shadow=False, zone=zone)
        # Roof slopes rise toward the centre along Y; the basic cube's local X is
        # the sloped direction and local Y spans the building's width.
        for side in (-1, 1):
            a = (x, y + side * (depth / 2 + 70), floor + height)
            b = (x, y, floor + height + roof_rise)
            run = math.hypot(a[0] - b[0], a[1] - b[1])
            self.box(ident + "_Roof", ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2 + 18),
                     (math.dist(a, b), width + 140, 36), "roof", yaw=math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])),
                     pitch=math.degrees(math.atan2(b[2] - a[2], run)), solid=True, zone=zone)
        for px in (x - width / 2 + 35, x + width / 2 - 35):
            for py in (y - depth / 2, y + depth / 2):
                self.box(ident + "_Corner", (px, py, floor + height / 2), (50, 44, height), "light_concrete", zone=zone)
        self.beam(ident + "_Ridge", (x - width / 2 - 65, y, floor + height + roof_rise + 30),
                  (x + width / 2 + 65, y, floor + height + roof_rise + 30), 24, "dark", zone=zone)
        self.report["buildings"].append(data)
        return data

    def ramp(self, ident, bottom, top, width, zone, mat="concrete", rails=True):
        delta = [top[i] - bottom[i] for i in range(3)]
        run = math.hypot(*delta[:2])
        slope = math.degrees(math.atan2(delta[2], run))
        assert 0 < abs(slope) <= 30, ident
        yaw = math.degrees(math.atan2(delta[1], delta[0]))
        normal = (-delta[0] / run * math.sin(math.radians(slope)),
                  -delta[1] / run * math.sin(math.radians(slope)), math.cos(math.radians(slope)))
        position = tuple((bottom[i] + top[i]) / 2 - normal[i] * 20 for i in range(3))
        self.box(ident, position, (math.dist(bottom, top), width, 40), mat, yaw=yaw, pitch=slope, solid=True, zone=zone)
        across = (-delta[1] / run, delta[0] / run)
        if rails:
            for sign in (-1, 1):
                a = (bottom[0] + across[0] * width / 2 * sign, bottom[1] + across[1] * width / 2 * sign, bottom[2] + 105)
                b = (top[0] + across[0] * width / 2 * sign, top[1] + across[1] * width / 2 * sign, top[2] + 105)
                self.beam(ident + "_Rail", a, b, 12, "yellow", zone=zone)
                # A thin solid guard follows the slope so actors cannot step sideways
                # off it. Top rail remains separately instanced decoration.
                centre = tuple((a[i] + b[i]) / 2 - (45 if i == 2 else 0) for i in range(3))
                self.box(ident + "_Guard", centre, (math.dist(bottom, top), 10, 90), "steel",
                         yaw=yaw, pitch=slope, solid=True, zone=zone)
        self.report["ramps"].append({"id": ident, "bottom_cm": list(bottom), "top_cm": list(top),
                                     "width_cm": width, "slope_degrees": abs(slope)})

    def table(self, ident, x, y, floor, zone, width=180):
        self.box(ident + "_Top", (x, y, floor + 89), (width, 95, 12), "wood", solid=True, zone=zone)
        for dx in (-width / 2 + 15, width / 2 - 15):
            for dy in (-33, 33):
                self.box(ident + "_Leg", (x + dx, y + dy, floor + 42), (8, 8, 84), "steel", zone=zone)

    def cover(self, ident, x, y, length=280, height=125, yaw=0, zone="warehouse"):
        self.box(ident, (x, y, height / 2), (length, 90, height), "concrete", yaw=yaw, solid=True, zone=zone)
        self.box(ident + "_Top", (x, y, height + 3), (length + 4, 94, 6), "light_concrete", yaw=yaw, zone=zone)
        a = math.radians(yaw)
        self.box(ident + "_Reflector", (x + 48 * math.sin(a), y - 48 * math.cos(a), height - 30),
                 (length * .65, 4, 25), "hazard", yaw=yaw, shadow=False, zone=zone)

    def crate(self, ident, xyz, size=(130, 110, 130), zone="warehouse", mat="wood"):
        x, y, floor = xyz
        width, depth, height = size
        self.box(ident, (x, y, floor + height / 2), size, mat, solid=True, zone=zone)
        for dz in (12, height - 12):
            self.box(ident + "_Band", (x, y - depth / 2 - 3, floor + dz), (width + 4, 5, 12), "dark", zone=zone)
        for dx in (-width / 2 + 10, width / 2 - 10):
            self.box(ident + "_Corner", (x + dx, y - depth / 2 - 4, floor + height / 2), (10, 7, height), "steel", zone=zone)

    def container(self, ident, x, y, zone, yaw=0, mat="blue", length=900):
        angle = math.radians(yaw)
        def loc(px, py, pz):
            return (x + px * math.cos(angle) - py * math.sin(angle), y + px * math.sin(angle) + py * math.cos(angle), pz)
        self.box(ident + "_Body", (x, y, 160), (length, 310, 320), mat, yaw=yaw, solid=True, zone=zone)
        for side in (-1, 1):
            for position in range(-int(length / 2) + 50, int(length / 2), 95):
                self.box(ident + "_Rib", loc(position, side * 157, 160), (10, 8, 290), mat, yaw=yaw, zone=zone)
            for z in (12, 310):
                self.box(ident + "_Rail", loc(0, side * 160, z), (length + 12, 12, 16), "dark", yaw=yaw, zone=zone)
        for side in (-1, 1):
            self.box(ident + "_Door", loc(length / 2 + 4, side * 75, 165), (8, 140, 285), mat, yaw=yaw, zone=zone)
            self.cylinder(ident + "_DoorRod", loc(length / 2 + 13, side * 95, 165), 7, 250, "steel", zone=zone)

    def terrain(self):
        # Five separate terrain panels leave a real hole for the lowered channel.
        panels = [(-3500, 0, 9000, 14000), (4500, 1400, 7000, 11200),
                  (4500, -5950, 7000, 2100), (7600, -4550, 800, 700)]
        for index, (x, y, width, depth) in enumerate(panels):
            self.box("Terrain_" + str(index), (x, y, -162), (width, depth, 300), "dirt", solid=True, zone="terrain")
        self.box("Drainage_Floor", (4100, -4550, -310), (6200, 700, 60), "wet", solid=True, zone="fuel")
        self.ramp("Plateau_WestRamp", (-1000, 4400, 0), (0, 4400, 300), 800, "comms", rails=False)
        self.ramp("Plateau_MainRamp", (4200, 2600, 0), (4200, 3600, 300), 800, "comms", rails=False)
        # Leave a recess in the south edge so the slab's vertical face does not
        # intersect the ascending main ramp before it reaches the upper surface.
        for ident, x, y, sx, sy in [("Back", 3500, 5050, 7000, 2900),
                                   ("West", 1900, 3300, 3800, 600), ("East", 5800, 3300, 2400, 600)]:
            self.box("Comms_Plateau_" + ident, (x, y, 144), (sx, sy, 312), "concrete", solid=True, zone="comms")
        # West ramp terminates at x=0, clear of the plateau's vertical west face.
        self.ramp("Drainage_WestRamp", (1000, -4550, 0), (2000, -4550, -280), 650, "fuel", rails=False)
        self.ramp("Drainage_EastRamp", (6200, -4550, -280), (7200, -4550, 0), 650, "fuel", rails=False)
        for y in (-4175, -4925):
            self.box("Drainage_RetainingWall", (4100, y, -130), (6200, 50, 300), "concrete", solid=True, zone="fuel")
            self.box("Drainage_Coping", (4100, y, 24), (6200, 70, 16), "light_concrete", zone="fuel")
        self.box("Drainage_Bridge", (4500, -4550, -20), (540, 850, 40), "concrete", solid=True, zone="fuel")
        for x in (4235, 4765):
            self.box("Bridge_Parapet", (x, -4550, 60), (20, 820, 120), "steel", solid=True, zone="fuel")
            self.box("Bridge_Handrail", (x, -4550, 127), (24, 840, 14), "yellow", zone="fuel")
            for y in (-4800, -4550, -4300):
                self.box("Bridge_Post", (x, y, 80), (24, 24, 160), "dark", zone="fuel")
        # Approach paths are short panels, not one large painted empty arena.
        roads = [(-4200, -5750, 5700, 900), (-1500, -3450, 8900, 700),
                 (-7350, -450, 700, 10300), (-5650, 1400, 3200, 700),
                 (-3000, 3050, 3500, 700), (2750, 300, 850, 4200),
                 (4200, 2350, 4200, 650), (7500, -3400, 650, 4700),
                 (6500, -5900, 2300, 850)]
        for index, (x, y, width, depth) in enumerate(roads):
            self.box("Road_" + str(index), (x, y, -18), (width, depth, 36), "asphalt", solid=True, zone="roads")
        for x in range(-6600, 2900, 550):
            self.box("Road_CentreDash", (x, -3450, 1.5), (180, 10, 3), "white", shadow=False, zone="roads")
        for y in range(-5200, 2500, 550):
            self.box("West_RouteDash", (-7350, y, 1.5), (10, 180, 3), "white", shadow=False, zone="roads")
        for x in range(5250, 7600, 350):
            self.box("SouthExit_Dash", (x, -5900, 1.5), (140, 12, 3), "yellow", shadow=False, zone="roads")
        # Boundary has low concrete plinths and transparent wire above, with
        # a physical collision surface matching the visible fence panel.
        for side in ("north", "south", "east", "west"):
            horizontal = side in ("north", "south")
            length = 16000 if horizontal else 14000
            fixed = (7000 if side == "north" else -7000) if horizontal else (8000 if side == "east" else -8000)
            segments = math.ceil(length / 800)
            for index in range(segments):
                at = -length / 2 + (index + .5) * length / segments
                x, y = (at, fixed) if horizontal else (fixed, at)
                span = length / segments
                # The northern platform is 3m above the yard. Its boundary must
                # also rise; a 3.7m fence could otherwise be cleared from there.
                fence_top = 650 if side == "north" and at >= 0 else 370
                wire_height = fence_top - 80
                self.box("Boundary_Base", (x, y, 40), (span, 55, 80) if horizontal else (55, span, 80),
                         "concrete", solid=True, instance=True, zone="boundary")
                self.box("Boundary_Wire", (x, y, 80 + wire_height / 2),
                         (span, 12, wire_height) if horizontal else (12, span, wire_height),
                         "fence", solid=True, shadow=False, instance=True, zone="boundary")
                px, py = (at - span / 2, fixed) if horizontal else (fixed, at - span / 2)
                self.box("Boundary_Post", (px, py, (fence_top + 10) / 2), (18, 18, fence_top + 10), "dark", zone="boundary")
                self.box("Boundary_Rail", (x, y, fence_top - 5), (span, 10, 10) if horizontal else (10, span, 10), "steel", zone="boundary")
        # Original industrial silhouettes beyond the inaccessible perimeter.
        for index, (x, y, sx, sy, height) in enumerate([
                (-5500, 8800, 6200, 2000, 1200), (2500, 9500, 7200, 2700, 1500),
                (10500, 1700, 2700, 6800, 1700), (-10300, 2500, 2300, 6300, 1000)]):
            self.box("Backdrop_Factory" + str(index), (x, y, height / 2), (sx, sy, height), "dark", zone="backdrop")
            self.box("Backdrop_Roof" + str(index), (x, y, height + 50), (sx + 100, sy + 100, 100), "roof", zone="backdrop")
            for offset in (-sx / 3, 0, sx / 3):
                self.box("Backdrop_Clerestory", (x + offset, y - sy / 2 - 4, height - 200), (550, 6, 160), "glass", zone="backdrop")
        for x, y, height in [(-2800, 8700, 2500), (7800, 9100, 3100), (10800, -2600, 2200)]:
            self.cylinder("Backdrop_Chimney", (x, y, height / 2), 280, height, "rust", zone="backdrop")
            for z in range(500, height, 700):
                self.cylinder("Backdrop_ChimneyBand", (x, y, z), 292, 120, "light_concrete", zone="backdrop")

    def support_and_medical(self):
        self.building("Gatehouse", (-6500, -4800), 1200, 1000, 360, "support",
                      {"south": [(0, 240, 270)], "east": [(0, 240, 270)]}, mat="light_concrete", roof_rise=80)
        self.building("Maintenance", (-4800, -2300), 2200, 1800, 510, "support",
                      {"south": [(100, 500, 380)], "east": [(0, 260, 280)]}, mat="olive")
        self.building("Medical", (-5900, 2800), 2400, 1700, 420, "medical",
                      {"south": [(0, 260, 280)], "east": [(50, 260, 280)]}, mat="plaster")
        self.building("Barracks", (-3800, 4700), 2400, 1600, 410, "medical",
                      {"west": [(-200, 240, 280)], "south": [(150, 240, 280)]}, mat="olive")
        self.table("Maintenance_Workbench", -4700, -2700, 0, "support", 250)
        self.table("Medical_Treatment", -6500, 3000, 0, "medical", 240)
        self.table("Barracks_Desk", -3100, 4500, 0, "medical", 200)
        # Functional interior subdivisions stop an uninterrupted view through rooms.
        self.wall("Medical_Partition", (-5850, 2800, 0), 1650, 310, 18, "y", [(0, 260, 260)], "light_concrete", "medical")
        self.wall("Barracks_Partition", (-3700, 4700, 0), 1560, 305, 18, "y", [(0, 240, 265)], "plaster", "medical")
        for x, y in [(-6750, 3350), (-6750, 2300), (-6250, 3350)]:
            self.box("Medical_CotFrame", (x, y, 40), (190, 78, 60), "steel", solid=True, zone="medical")
            self.box("Medical_Mattress", (x, y, 77), (184, 72, 18), "olive", zone="medical")
            self.box("Medical_Pillow", (x - 60, y, 92), (45, 65, 13), "light_concrete", zone="medical")
        for x in (-4550, -4200, -3200, -2900):
            self.box("Barracks_Bunk", (x, 5250, 48), (210, 95, 96), "dark", solid=True, zone="medical")
            self.box("Barracks_Bedding", (x, 5250, 100), (204, 89, 16), "olive", zone="medical")
        for x in (-5520, -5280, -5040):
            self.box("Maintenance_Locker", (x, -1480, 115), (140, 75, 230), "blue", solid=True, zone="support")
            self.box("Maintenance_LockerHandle", (x - 40, -1522, 115), (6, 6, 35), "steel", zone="support")
        for x, y, floor, zone in [(-6600, -4500, 0, "support"), (-5050, 3500, 0, "medical"), (-3050, 4200, 0, "medical")]:
            self.box("WallCabinet", (x, y, floor + 140), (200, 65, 280), "olive", solid=True, zone=zone)
            self.box("WallCabinet_Handle", (x, y - 35, floor + 130), (8, 8, 45), "steel", zone=zone)
        # Porch roofs, signage and coloured building trim establish distinct zones.
        self.box("Medical_Canopy", (-5900, 1740, 310), (700, 450, 30), "dark", zone="medical")
        for x in (-6200, -5600):
            self.box("Medical_PorchPost", (x, 1560, 155), (18, 18, 310), "steel", solid=True, zone="medical")
        self.box("Medical_CrossV", (-6280, 1924, 330), (30, 12, 130), "light_concrete", zone="medical")
        self.box("Medical_CrossH", (-6280, 1920, 330), (130, 12, 30), "light_concrete", zone="medical")
        self.sign("Gatehouse_Title", "后勤检查站", (-6500, -5324, 308), size=46, zone="support")
        self.sign("Maintenance_Title", "维修车间", (-4800, -3224, 445), size=60, zone="support")
        self.sign("Medical_Title", "医疗站", (-5810, 1918, 350), size=58, zone="medical")
        self.sign("Barracks_Title", "驻防营房", (-3550, 3874, 340), size=48, zone="medical")
        self.sign("Insertion_Brief", "边境工业站  /  搜集物资\n通讯站位于北侧台地", (-6354, -5962, 190), yaw=-115, size=38, zone="support")
        self.box("Insertion_BriefBoard", (-6350, -5950, 190), (750, 18, 170), "dark", yaw=-25, solid=True, zone="support")
        # Low utility shelter makes the final safe support cache a place to inspect.
        for x in (-6800, -6400):
            for y in (-950, -450):
                self.box("Support_ShelterPost", (x, y, 180), (16, 16, 360), "steel", solid=True, zone="support")
        self.box("Support_ShelterRoof", (-6600, -700, 370), (520, 640, 24), "olive", zone="support")
        self.box("Support_Generator", (-6400, -700, 85), (130, 250, 170), "olive", solid=True, zone="support")
        self.pipe("Support_GeneratorExhaust", (-6400, -620, 150), (-6400, -620, 320), 20, "dark", zone="support")
        for x, y, sx, sy in [(-6700, -6450, 800, 90), (-6600, -3850, 900, 90), (-5500, 800, 900, 100)]:
            self.box("Support_BlastWall", (x, y, 100), (sx, sy, 200), "concrete", solid=True, zone="support")
        for index, (xyz, zone) in enumerate([((-6500, -4800, 325), "support"), ((-4800, -2300, 465), "support"),
                                               ((-6300, 2800, 380), "medical"), ((-5250, 2800, 380), "medical"),
                                               ((-3800, 4700, 370), "medical")]):
            self.lamp("Support_InteriorLamp" + str(index), xyz, zone, radius=1350, intensity=1350)

    def warehouse(self):
        data = self.building("Warehouse", (-300, 200), 4200, 3200, 730, "warehouse",
                             {"south": [(-700, 600, 460)], "east": [(200, 500, 470)], "west": [(300, 240, 290)]},
                             mat="concrete", roof_rise=180)
        data["floor_heights_cm"].append(400)
        self.box("Warehouse_Mezzanine", (-250, 1350, 380), (3300, 700, 40), "concrete", solid=True, zone="warehouse")
        self.ramp("Warehouse_MezzanineRamp", (1200, 0, 0), (1200, 1000, 400), 260, "warehouse", "steel")
        for x in (-1850, -600, 650, 1350):
            self.box("Warehouse_MezzanineSupport", (x, 1650, 190), (36, 36, 380), "steel", solid=True, zone="warehouse")
        for a, b in [((-1900, 1000), (1000, 1000)), ((1400, 1000), (1400, 1700)), ((-1900, 1000), (-1900, 1700))]:
            self.beam("Warehouse_MezzanineRail", (*a, 515), (*b, 515), 14, "yellow", zone="warehouse")
            self.beam("Warehouse_MezzanineMidRail", (*a, 460), (*b, 460), 8, "steel", zone="warehouse")
            distance = math.dist(a, b)
            for index in range(math.ceil(distance / 180) + 1):
                t = min(1, index * 180 / distance)
                self.box("Warehouse_MezzaninePost", (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, 455),
                         (12, 12, 110), "steel", zone="warehouse")
            self.beam("Warehouse_MezzanineToe", (*a, 412), (*b, 412), 22, "dark", zone="warehouse")
        # Lower solid parapets leave firing windows while preventing falls along
        # the long front edge. The ramp landing at x1200 is deliberately open.
        self.box("Warehouse_MezzanineGuard", (-450, 1000, 450), (2900, 16, 100), "dark", solid=True, zone="warehouse")
        for x in (-1750, -350, 1100):
            self.beam("Warehouse_RoofTruss", (x, -1380, 700), (x, 200, 870), 24, "steel", zone="warehouse")
            self.beam("Warehouse_RoofTruss", (x, 200, 870), (x, 1780, 700), 24, "steel", zone="warehouse")
            self.beam("Warehouse_TieBeam", (x, -1350, 690), (x, 1750, 690), 22, "steel", zone="warehouse")
        for x, y in [(-2180, -1000), (-2180, 1100), (1640, -1000), (1640, 1200)]:
            self.box("Warehouse_Column", (x, y, 360), (45, 45, 720), "steel", solid=True, zone="warehouse")
        self.table("Warehouse_Inspection", -900, 350, 0, "warehouse", 230)
        self.crate("Warehouse_MachineCover", (-700, 750, 0), (480, 260, 230), "warehouse", "olive")
        self.crate("Warehouse_PalletCover", (-450, -600, 0), (320, 220, 195), "warehouse")
        self.crate("Warehouse_WestStock", (-2070, -500, 0), (290, 420, 230), "warehouse")
        self.crate("Warehouse_MezzanineStock", (-1600, 1450, 400), (230, 220, 175), "warehouse", "olive")
        self.box("Warehouse_MezzanineDivider", (-250, 1540, 530), (30, 290, 260), "blue", solid=True, zone="warehouse")
        self.container("Loading_BlueContainer", -1800, -2800, "warehouse", yaw=0, mat="blue", length=1000)
        self.container("Loading_RustContainer", 1700, -2600, "warehouse", yaw=0, mat="rust", length=1000)
        self.container("Loading_OliveContainer", -3150, 300, "warehouse", yaw=90, mat="olive", length=850)
        for index, (x, y, yaw, height) in enumerate([(-500, -1950, 0, 125), (2650, 1400, 0, 190),
                                                   (-2900, -1050, 90, 170), (3500, -1100, 90, 130)]):
            self.cover("Warehouse_ApproachCover" + str(index), x, y, height=height, yaw=yaw)
        self.box("Warehouse_EntryAwning", (-1000, -1590, 490), (820, 410, 30), "blue", zone="warehouse")
        self.sign("Warehouse_Title", "中央仓储  /  高价值物资", (-300, -1430, 600), size=70, zone="warehouse")
        self.sign("Warehouse_UpperSign", "夹层仓储  ↑", (1590, -380, 260), yaw=180, size=37, zone="warehouse")
        self.sign("Warehouse_Caution", "交火高风险区", (-1750, -1630, 245), size=42, color=(241, 187, 82, 255), zone="warehouse")
        self.sign("Warehouse_EastExit", "燃料站  →", (1770, 430, 555), yaw=0, size=40, zone="warehouse")
        for index, xyz in enumerate([(-1200, -500, 620), (600, -400, 620), (-1100, 1300, 690), (750, 1350, 690)]):
            self.lamp("Warehouse_InteriorLamp" + str(index), xyz, "warehouse", radius=1500, intensity=2500)

    def fuel(self):
        self.building("PumpHouse", (4800, -3100), 2000, 1500, 470, "fuel",
                      {"west": [(100, 260, 300)], "north": [(0, 300, 300)]}, mat="rust", roof_rise=100)
        self.table("PumpHouse_Workbench", 5350, -2800, 0, "fuel", 200)
        for index, (x, y, diameter, height) in enumerate([(5400, -600, 850, 850), (6600, -800, 830, 950), (5800, 850, 900, 900)]):
            self.cylinder("Fuel_Tank" + str(index), (x, y, height / 2), diameter, height, "steel", solid=True, zone="fuel")
            self.shape("Fuel_TankDome", (x, y, height), (diameter, diameter, 220), "steel", mesh="Sphere", zone="fuel")
            self.cylinder("Fuel_TankFoundation", (x, y, 18), diameter + 130, 36, "concrete", solid=True, zone="fuel")
            for z in (80, height * .5, height - 40):
                self.cylinder("Fuel_TankBand", (x, y, z), diameter + 12, 28, "dark", zone="fuel")
            self.box("Fuel_TankWarning", (x, y - diameter / 2 - 10, 460), (230, 10, 125), "yellow", zone="fuel")
            self.sign("Fuel_TankNumber" + str(index), "燃料  0" + str(index + 1), (x, y - diameter / 2 - 18, 470), size=37, color=(40, 45, 37, 255), zone="fuel")
            self.pipe("Fuel_TankOutlet", (x - diameter / 2, y, 100), (x - diameter / 2 - 220, y, 100), 45, "rust", zone="fuel")
            self.pipe("Fuel_TankVertical", (x - diameter / 2 - 220, y, 100), (x - diameter / 2 - 220, y, 420), 45, "rust", zone="fuel")
        # Raised pipe rack crosses an open route at 4.5m, with supports away from
        # navigation anchors. Large cylinders provide meaningful visual identity.
        for x in (3550, 7350):
            for y in (-1800, 1800):
                self.box("Fuel_PipeRackPost", (x, y, 235), (32, 32, 470), "steel", solid=True, zone="fuel")
        for y in (-1800, 1800):
            self.beam("Fuel_PipeRackHeader", (3550, y, 470), (7350, y, 470), 34, "dark", zone="fuel")
        for x in (3650, 3740, 3830):
            self.pipe("Fuel_OverheadPipe", (x, -1800, 510), (x, 1800, 510), 42, "rust", zone="fuel")
        self.pipe("Fuel_PumpFeed", (4000, -2300, 420), (4000, -1800, 420), 48, "steel", zone="fuel")
        self.pipe("Fuel_PumpElbow", (4000, -2300, 130), (4000, -2300, 420), 48, "steel", zone="fuel")
        for x, y in [(4200, -2850), (3900, 1750), (6300, 1550)]:
            self.box("Fuel_ControlCabinet", (x, y, 120), (150, 85, 240), "olive", solid=True, zone="fuel")
            self.box("Fuel_ControlPanel", (x, y - 44, 160), (105, 5, 65), "dark", zone="fuel")
            for dx in (-28, 0, 28):
                self.box("Fuel_ControlSwitch", (x + dx, y - 48, 165), (12, 6, 12), "yellow", zone="fuel")
        for index, (x, y, yaw, height) in enumerate([(6200, -3550, 0, 170), (3900, 900, 90, 190),
                                                   (5050, 1700, 0, 125), (7150, -2750, 90, 170)]):
            self.cover("Fuel_Cover" + str(index), x, y, length=360, height=height, yaw=yaw, zone="fuel")
        self.sign("Fuel_Title", "燃料泵站", (4800, -2325, 385), yaw=90, size=58, zone="fuel")
        self.sign("Drainage_Title", "排水通道  /  南侧撤离", (2500, -4160, 190), yaw=90, size=44, zone="fuel")
        self.sign("Drainage_Low", "桥下通道", (4150, -4550, -90), yaw=180, size=30, zone="fuel")
        self.sign("SouthExit_Direction", "南侧公路撤离  →", (7410, -3450, 215), yaw=180, size=40, color=(117, 221, 171, 255), zone="fuel")
        self.lamp("PumpHouse_InteriorLamp", (4800, -3100, 425), "fuel", radius=1300, intensity=1650)
        # A low cable pipe and wall gratings punctuate the channel without
        # adding collisions at ankle height or blocking its navigation corridor.
        self.pipe("Drainage_ServicePipe", (2000, -4840, -175), (6200, -4840, -175), 24, "rust", zone="fuel")
        for x in (2600, 3700, 5200, 6000):
            self.box("Drainage_Grating", (x, -4194, -110), (150, 8, 110), "dark", zone="fuel")
            for dx in (-50, -25, 0, 25, 50):
                self.box("Drainage_GrateBar", (x + dx, -4200, -110), (9, 8, 105), "steel", shadow=False, zone="fuel")

    def comms(self):
        data = self.building("CommsControl", (1800, 4500), 2400, 1800, 870, "comms",
                             {"south": [(0, 300, 300)], "east": [(0, 280, 300)]},
                             floor=300, mat="light_concrete", roof_rise=100)
        data["floor_heights_cm"].append(700)
        self.box("Comms_UpperFloor", (1800, 5030, 680), (2280, 660, 40), "concrete", solid=True, zone="comms")
        self.ramp("Comms_InteriorRamp", (1100, 3900, 300), (1100, 4700, 700), 260, "comms", "steel")
        self.box("Comms_UpperGuard", (2110, 4700, 755), (1620, 18, 110), "dark", solid=True, zone="comms")
        self.box("Comms_UpperRail", (2110, 4700, 816), (1620, 22, 12), "yellow", zone="comms")
        self.box("Comms_UpperPartition", (2430, 5120, 820), (22, 420, 240), "olive", solid=True, zone="comms")
        for x in (1450, 2300, 2830):
            self.box("Comms_UpperSupport", (x, 5250, 490), (30, 30, 380), "steel", solid=True, zone="comms")
        for x, y in [(780, 4250), (2680, 4550), (1800, 5310)]:
            self.box("Comms_ServerRack", (x, y, 440), (150, 100, 280), "dark", solid=True, zone="comms")
            for z in (360, 410, 460, 510):
                self.box("Comms_ServerDrawer", (x, y - 54, z), (132, 6, 34), "steel", zone="comms")
                self.box("Comms_ServerLED", (x - 45, y - 58, z + 4), (8, 3, 6), "green", shadow=False, zone="comms")
        self.box("Comms_MapPanel", (2982, 4010, 505), (6, 270, 140), "dark", zone="comms")
        self.sign("Comms_Title", "通讯控制站", (1800, 3574, 860), size=76, zone="comms")
        self.sign("Comms_UpperGuide", "通讯终端  /  上层 ↑", (1490, 3630, 605), yaw=90, size=39, zone="comms")
        self.sign("Comms_TerminalLabel", "情报下载终端", (2200, 5368, 970), size=37, zone="comms")
        self.sign("Comms_Approach", "通讯台地  ↑", (3690, 2740, 290), size=48, color=(232, 184, 98, 255), zone="comms")
        self.sign("Comms_WestApproach", "通讯站  →", (-1080, 4880, 220), yaw=180, size=40, zone="comms")
        # Eighteen-metre lattice tower: four physical legs, lightweight repeated
        # braces, equipment deck and antenna are all original primitive assembly.
        tx, ty, base, height = 4300, 4700, 300, 1800
        for dx in (-230, 230):
            for dy in (-230, 230):
                self.box("Comms_TowerFoot", (tx + dx, ty + dy, base + 20), (130, 130, 40), "concrete", solid=True, zone="comms")
                self.beam("Comms_TowerLeg", (tx + dx, ty + dy, base),
                          (tx + dx * .45, ty + dy * .45, base + height), 32, "steel", solid=True, zone="comms")
        for index in range(5):
            z0, z1 = base + index * 350, base + (index + 1) * 350
            half0, half1 = 230 * (1 - index * .11), 230 * (1 - (index + 1) * .11)
            for side in (-1, 1):
                self.beam("Comms_TowerBrace", (tx - half0, ty + side * half0, z0), (tx + half1, ty + side * half1, z1), 12, "steel", zone="comms")
                self.beam("Comms_TowerBrace", (tx + half0, ty + side * half0, z0), (tx - half1, ty + side * half1, z1), 12, "steel", zone="comms")
                self.beam("Comms_TowerBrace", (tx + side * half0, ty - half0, z0), (tx + side * half1, ty + half1, z1), 12, "steel", zone="comms")
                self.beam("Comms_TowerRing", (tx - half0, ty + side * half0, z0), (tx + half0, ty + side * half0, z0), 16, "dark", zone="comms")
        self.box("Comms_TowerDeck", (tx, ty, base + 1450), (510, 510, 28), "steel", zone="comms")
        self.cylinder("Comms_TowerMast", (tx, ty, base + 1890), 24, 410, "steel", zone="comms")
        self.shape("Comms_AntennaDish", (tx + 205, ty, base + 1430), (170, 55, 170), "light_concrete", mesh="Sphere", yaw=25, zone="comms")
        for yaw in (0, 90, 180, 270):
            a = math.radians(yaw)
            self.box("Comms_PanelAntenna", (tx + math.cos(a) * 145, ty + math.sin(a) * 145, base + 1810),
                     (35, 85, 280), "light_concrete", yaw=yaw, zone="comms")
        self.cylinder("Comms_TowerBeacon", (tx, ty, base + 2110), 40, 30, "red_light", shadow=False, zone="comms")
        # The tower's service cache is beside its legs, not trapped inside them.
        self.box("Comms_TowerCabinet", (4100, 4390, 420), (165, 85, 240), "olive", solid=True, zone="comms")
        self.box("Comms_Generator", (5250, 4800, 395), (260, 370, 190), "olive", solid=True, zone="comms")
        self.pipe("Comms_GeneratorExhaust", (5350, 4900, 470), (5350, 4900, 760), 25, "dark", zone="comms")
        self.box("Comms_SwitchWall", (5550, 5100, 425), (300, 55, 250), "concrete", solid=True, zone="comms")
        self.box("Comms_BlastWall", (3670, 4080, 410), (65, 680, 220), "concrete", solid=True, zone="comms")
        self.box("Comms_RailCover", (6100, 4800, 395), (650, 80, 190), "concrete", solid=True, zone="comms")
        # Railway beyond the northern extraction anchors the exit's name.
        for y in (6460, 6600):
            self.box("Rail_Line", (3600, y, 325), (9000, 16, 26), "steel", zone="comms")
        for x in range(-500, 7900, 140):
            self.box("Rail_Sleeper", (x, 6530, 303), (32, 280, 18), "wood", zone="comms")
        for index, xyz in enumerate([(1800, 4050, 1030), (1650, 5100, 1100), (2650, 5050, 1100)]):
            self.lamp("Comms_InteriorLamp" + str(index), xyz, "comms", radius=1350, intensity=1800)

    def route_details(self):
        # Useful distinct roadside silhouettes: cable reels, electric cabinets,
        # poles and gantries instead of evenly scattering identical supply crates.
        for x, y, zone in [(-6100, -3350, "support"), (-3500, 2250, "medical"), (3000, -2900, "warehouse")]:
            self.cylinder("CableReel_Core", (x, y, 85), 90, 110, "dark", pitch=90, zone=zone)
            for dx in (-62, 62):
                self.cylinder("CableReel_Flange", (x + dx, y, 85), 155, 15, "wood", pitch=90, solid=True, zone=zone)
        for index, (x, y, zone) in enumerate([(-7050, -3550, "support"), (-7050, 900, "medical"),
                                            (-3000, 2000, "warehouse"), (3150, -3900, "fuel"), (7150, 2400, "fuel")]):
            self.box("PowerPole" + str(index), (x, y, 450), (28, 28, 900), "dark", solid=True, zone=zone)
            self.box("PowerPole_Crossbar", (x, y, 850), (280, 18, 18), "steel", zone=zone)
            for dx in (-100, 0, 100):
                self.cylinder("PowerPole_Insulator", (x + dx, y, 880), 25, 45, "light_concrete", zone=zone)
        for a, b in [((-7050, -3550, 885), (-7050, 900, 885)), ((-7050, 900, 885), (-3000, 2000, 885)),
                     ((3150, -3900, 885), (7150, 2400, 885))]:
            for dx in (-100, 100):
                mid = ((a[0] + b[0]) / 2 + dx, (a[1] + b[1]) / 2, 710)
                self.beam("Overhead_Cable", (a[0] + dx, a[1], a[2]), mid, 5, "rubber", shadow=False, zone="roads")
                self.beam("Overhead_Cable", mid, (b[0] + dx, b[1], b[2]), 5, "rubber", shadow=False, zone="roads")
        for ident, centre, text, yaw in [
                ("Junction_West", (-3300, -3650, 190), "中央仓储 ↑\n医疗营房 ←", -90),
                ("Junction_North", (-1900, 3000, 190), "医疗营房 ←  通讯站 →", -90),
                ("Junction_Fuel", (3100, 2100, 190), "通讯台地 ↑\n燃料泵站 ↓", 180)]:
            self.box(ident + "_Board", centre, (700, 18, 160), "dark", yaw=yaw + 90, zone="roads")
            self.sign(ident, text, (centre[0] + (14 if yaw == 0 else -14 if yaw == 180 else 0),
                                    centre[1] - (14 if yaw == -90 else 0), centre[2]), yaw=yaw, size=38, zone="roads")
            self.box(ident + "_Post", (centre[0], centre[1], 80), (20, 20, 160), "steel", zone="roads")
        for index, (x, y, zone) in enumerate([(-6200, -5650, "support"), (-3600, -3700, "support"),
                                              (-6900, 1500, "medical"), (3100, 1800, "warehouse"), (7300, -5400, "fuel")]):
            self.box("RoadLamp_Post", (x, y, 310), (20, 20, 620), "dark", solid=True, zone=zone)
            self.beam("RoadLamp_Arm", (x, y, 620), (x + 100, y, 640), 14, "steel", zone=zone)
            self.lamp("RoadLamp" + str(index), (x + 100, y, 633), zone, radius=1200, intensity=700)

    def gameplay(self):
        u = self.u
        player_class = self.assets.load_blueprint_class("/Game/Blueprints/BP_RaidPlayer")
        enemy_class = self.assets.load_blueprint_class("/Game/Blueprints/BP_DepotGuard")
        loot_class = self.assets.load_blueprint_class("/Game/Blueprints/BP_DepotSupply")
        extraction_class = self.assets.load_blueprint_class("/Game/Blueprints/BP_DepotExtraction")
        game_class = self.assets.load_blueprint_class("/Game/Blueprints/BP_DepotGameMode")
        assert all((player_class, enemy_class, loot_class, extraction_class, game_class)), "Existing gameplay Blueprint assets required"
        world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", game_class)
        self.tag(self.actors.spawn_actor_from_class(u.PlayerStart, u.Vector(*SPAWN), self.rotation(35)), "R7_Insertion", "support", "gameplay")
        for ident, zone, xyz, item, label, value in LOOT:
            actor = self.tag(self.actors.spawn_actor_from_class(loot_class, u.Vector(*xyz)), ident, zone, "loot")
            for key, val in [("item_id", u.Name(item)), ("display_name", label), ("quantity", 1), ("value", value),
                             ("generate_supplemental_loot", False), ("search_duration", 1.35 if value < 200 else 1.8)]:
                actor.set_editor_property(key, val)
            # One valuable row plus one modest supply row keeps the eight-slot
            # backpack meaningful without stuffing every case with eight items.
            seeds = []
            for entry_item, entry_label, entry_value in [(item, label, value),
                    (("Ammo", "散装备用弹药", 25) if item == "Tech" else ("Rations", "应急口粮", 25))]:
                entry = u.RaidLootEntry()
                entry.set_editor_property("item_id", u.Name(entry_item))
                entry.set_editor_property("display_name", entry_label)
                entry.set_editor_property("quantity", 1)
                entry.set_editor_property("value", entry_value)
                seeds.append(entry)
            actor.set_editor_property("contents", seeds)
            self.report["loot"].append({"id": ident, "actor_label": ident, "actor_tag": "R7Id:" + ident, "zone": zone,
                                         "location_cm": list(xyz), "item_id": item, "value": value, "rows": 2})
        for ident, zone, xyz, patrol, detection, attack in GUARDS:
            actor = self.tag(self.actors.spawn_actor_from_class(enemy_class, u.Vector(*xyz), self.rotation(90)), ident, zone, "guards")
            actor.set_editor_property("patrol_points", [u.Vector(*point) for point in patrol])
            actor.set_editor_property("detection_radius", detection)
            actor.set_editor_property("attack_range", attack)
            actor.set_editor_property("attack_interval", 1.45 if zone in ("support", "medical") else 1.3)
            self.report["guards"].append({"id": ident, "actor_label": ident, "actor_tag": "R7Id:" + ident, "zone": zone,
                                           "location_cm": list(xyz), "patrol_points_cm": [list(p) for p in patrol],
                                           "detection_radius_cm": detection, "attack_range_cm": attack})
        self.assets.make_directory("/Game/Data")
        mission_path = "/Game/Data/DA_FrontierIntel"
        mission = self.assets.load_asset(mission_path) if self.assets.does_asset_exist(mission_path) else None
        if mission is None:
            factory = u.DataAssetFactory()
            factory.set_editor_property("data_asset_class", u.RaidMissionDefinition)
            mission = u.AssetToolsHelpers.get_asset_tools().create_asset("DA_FrontierIntel", "/Game/Data", u.RaidMissionDefinition, factory)
        assert mission, "Could not create Frontier mission definition"
        for key, val in [("mission_id", u.Name("FrontierIntel")), ("title", "下载边境通讯情报"),
                         ("instruction", "前往通讯站，长按 E 下载情报"), ("completed_text", "情报下载完成，北侧撤离点已解锁"),
                         ("hold_seconds", 6.0), ("interaction_range", 260.0)]:
            mission.set_editor_property(key, val)
        assert self.assets.save_loaded_asset(mission, only_if_is_dirty=False), "Mission definition did not save"
        objective = self.tag(self.actors.spawn_actor_from_class(u.RaidMissionObjective, u.Vector(*MISSION), self.rotation(180)),
                             "FrontierIntel", "comms", "mission")
        objective.set_actor_label("R7_Objective_FrontierIntel")
        objective.set_editor_property("mission_definition", mission)
        self.report["mission"] = {"id": "FrontierIntel", "actor_label": objective.get_actor_label(), "actor_tag": "R7Id:FrontierIntel",
                                  "definition": mission_path, "location_cm": list(MISSION), "hold_seconds": 6,
                                  "interaction_stand_cm": [1980, 4900, 700], "entry_route_id": "comms_upper"}
        for spec in EXTRACTIONS:
            xyz = spec["location_cm"]
            zone = "fuel" if spec["id"] == "S-01" else "comms"
            actor = self.tag(self.actors.spawn_actor_from_class(extraction_class, u.Vector(*xyz)), spec["id"], zone, "extractions")
            actor.set_editor_property("extraction_id", u.Name(spec["id"]))
            actor.set_editor_property("display_name", spec["name"])
            actor.set_editor_property("required_mission_id", u.Name(spec["required_mission_id"] or "None"))
            actor.set_editor_property("extract_seconds", 8.0)
            floor = 0 if spec["id"] == "S-01" else 300
            x, y = xyz[:2]
            self.box("Extraction_Canopy", (x, y, floor + 390), (730, 750, 28), "olive", zone=zone)
            for dx in (-345, 345):
                for dy in (-345, 345):
                    self.box("Extraction_Post", (x + dx, y + dy, floor + 195), (18, 18, 390), "steel", solid=True, zone=zone)
            # A back wall defines the checkpoint but leaves the complete 520cm
            # trigger footprint unobstructed, including its native signal cabinet.
            self.box("Extraction_Backstop", (x, y + 370, floor + 145), (780, 35, 290), "concrete", solid=True, zone=zone)
            self.sign("Extraction_Direction_" + spec["id"], spec["name"] + " / " + spec["id"],
                      (x, y - 380, floor + 350), size=42, color=(128, 219, 171, 255), zone=zone)
            self.report["extractions"].append(dict(spec, actor_label=spec["id"], actor_tag="R7Id:" + spec["id"]))
        nav = self.tag(self.actors.spawn_actor_from_class(u.RaidNavBounds, u.Vector(0, 0, 500)), "R7_Navigation", "terrain", "navigation")
        extent = u.Vector(8100, 7100, 1300)
        nav.set_editor_property("bounds_extent", extent)
        nav.get_editor_property("bounds_box").set_box_extent(extent, False)
        self.report["navigation"] = {"location_cm": [0, 0, 500], "extent_cm": [8100, 7100, 1300],
                                     "agent_radius_cm": 42, "agent_height_cm": 192}
        for route in self.report["routes"]:
            route["anchor_tags"] = []
            for index, point in enumerate(route["waypoints_cm"]):
                route_tag = "R7Route:" + route["id"] + ":" + str(index)
                actor = self.tag(self.actors.spawn_actor_from_class(u.TargetPoint, u.Vector(*point)),
                                 "Route_" + route["id"] + "_" + str(index), "validation", "route_anchors")
                actor.set_editor_property("tags", list(actor.get_editor_property("tags")) + [u.Name(route_tag)])
                route["anchor_tags"].append(route_tag)

    def lighting(self):
        u = self.u
        sun = self.tag(self.actors.spawn_actor_from_class(u.DirectionalLight, u.Vector(0, 0, 2500), self.rotation(-38, -48)),
                       "Frontier_Sun", "lighting", "lights")
        sun.light_component.set_mobility(u.ComponentMobility.MOVABLE)
        sun.light_component.set_intensity(3.1)
        sun.light_component.set_light_color(u.LinearColor(1.0, .94, .84, 1))
        sun.light_component.set_editor_property("light_source_angle", 2.0)
        sky = self.tag(self.actors.spawn_actor_from_class(u.SkyLight, u.Vector(0, 0, 1200)), "Frontier_Sky", "lighting", "lights")
        cube = u.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap")
        assert cube, "Installed daylight cubemap required"
        sky.light_component.set_mobility(u.ComponentMobility.MOVABLE)
        sky.light_component.set_editor_property("real_time_capture", False)
        sky.light_component.set_editor_property("source_type", u.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        sky.light_component.set_cubemap(cube)
        sky.light_component.set_intensity(1.05)
        sky.light_component.set_editor_property("lower_hemisphere_is_black", False)
        self.tag(self.actors.spawn_actor_from_class(u.SkyAtmosphere, u.Vector()), "Frontier_Atmosphere", "lighting", "lights")
        fog = self.tag(self.actors.spawn_actor_from_class(u.ExponentialHeightFog, u.Vector(0, 0, -300)), "Frontier_Fog", "lighting", "lights")
        fog.get_component_by_class(u.ExponentialHeightFogComponent).set_editor_property("fog_density", .003)

    def execute(self):
        if self.assets.does_asset_exist(MAP):
            assert self.levels.load_level(MAP), "Could not load existing Frontier"
            for actor in list(self.actors.get_all_level_actors()):
                if TAG in [str(tag) for tag in actor.get_editor_property("tags")]:
                    assert self.actors.destroy_actor(actor), "Could not replace Frontier generated actor"
        else:
            assert self.levels.new_level(MAP), "Could not create Frontier"
        self.terrain()
        self.support_and_medical()
        self.warehouse()
        self.fuel()
        self.comms()
        self.route_details()
        self.gameplay()
        self.lighting()
        # ISM assignment can enable material usage flags. Persist those flags for
        # cooked builds rather than relying on an editor-only automatic repair.
        for material in self.materials.values():
            # The retained chain-link material is outside /Materials/R7 too.
            unreal.MaterialEditingLibrary.set_base_material_usage(
                material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
            assert self.assets.save_loaded_asset(material, only_if_is_dirty=True)
        for view in self.report["preview_views"]:
            view.setdefault("show_tactical_map", False)
        self.report["counts"] = dict(self.counts)
        self.report["counts"]["environment_actors"] = sum(value for key, value in self.counts.items()
            if key.endswith("_actors") and key not in {"loot_actors", "guards_actors", "extractions_actors", "mission_actors", "route_anchors_actors", "gameplay_actors"})
        assert self.report["counts"]["environment_actors"] < 900, self.report["counts"]
        assert len(self.report["loot"]) == 24 and len(self.report["guards"]) == 12
        assert len(self.report["extractions"]) == 2 and len(self.report["buildings"]) == 7
        assert self.levels.save_current_level(), "Frontier map did not save"
        root = Path(self.u.Paths.project_dir()).resolve()
        payload = json.dumps(self.report, ensure_ascii=False, indent=2).encode("utf-8")
        for relative in ("Evidence/R7MapBuild.json", "Content/Validation/FrontierValidation.json"):
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
        self.u.log("R7_MAP_BUILD_COMPLETE: " + MAP + " " + json.dumps(self.report["counts"]) +
                   "; 160x140m; 7 enterable buildings; 24 containers; 12 guards; 2 exits; metadata saved twice")


def build_frontier():
    import unreal
    scripts = str(Path(__file__).resolve().parent)
    if scripts not in sys.path:
        sys.path.insert(0, scripts)
    from r7_visual_assets import MATERIAL_PATHS, build_materials
    assert hasattr(unreal, "RaidPropCluster") and hasattr(unreal, "RaidMissionObjective"), "Compile the R7 editor target before generating Frontier"
    materials = {key: unreal.load_asset(path) for key, path in MATERIAL_PATHS.items()}
    if not all(materials.values()):
        materials = build_materials()
    else:
        unreal.log("R7_MAP_MATERIALS: reuse existing R7 material assets; run r7_visual_assets.py explicitly when the material specification changes")
    FrontierBuilder(unreal, materials).execute()


if __name__ == "__main__":
    build_frontier()


