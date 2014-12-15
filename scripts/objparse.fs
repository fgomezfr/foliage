module Foliage.Exporter

open System
open System.IO

exception ExportError

let parseObj (f : string) =
    let lines = File.ReadAllLines(f)

    let lines = Array.filter (fun (s : string) -> not (s.StartsWith("#") || s.StartsWith("u") || s.StartsWith("m") || (String.length s = 0))) lines

    let (vertices, rest) = Array.partition (fun (s : string) -> s.StartsWith("v ")) lines
    let vertices = vertices
                   |> Array.map (fun (s : string) -> s.Substring(2, String.length s - 2))
                   |> Array.map (fun (s : string) -> s.Split(' '))
                   |> Array.map (fun (a : string[]) -> (System.Single.Parse(Array.get a 0), System.Single.Parse(Array.get a 1), System.Single.Parse(Array.get a 2)))

    let (normals, rest) = Array.partition (fun (s : string) -> s.StartsWith("vn ")) rest
    let normals = normals
                   |> Array.map (fun (s : string) -> s.Substring(3, String.length s - 3))
                   |> Array.map (fun (s : string) -> s.Split(' '))
                   |> Array.map (fun (a : string[]) -> (System.Single.Parse(Array.get a 0), System.Single.Parse(Array.get a 1), System.Single.Parse(Array.get a 2)))

    let (texcoords, rest) = Array.partition (fun (s : string) -> s.StartsWith("vt ")) rest
    let texcoords = texcoords
                    |> Array.map (fun (s : string) -> s.Substring(3, String.length s - 3))
                    |> Array.map (fun (s : string) -> s.Split(' '))
                    |> Array.map (fun (a : string[]) -> (System.Single.Parse(Array.get a 0), System.Single.Parse(Array.get a 1)))

    let groups = rest

    let groupfolder ((groupname: string, faces : (int * int * int)[] list), groups) (line : string) =
        if line.StartsWith("g ")
        then // beginning of a new group, close the old one
          let nextGroupName = line.Substring(2, String.length line - 2)
          match faces with
          | [] -> ((nextGroupName, []), groups) // skip empty groups
          | _ -> ((nextGroupName, []), (groupname, faces)::groups)
        else if line.StartsWith("f ")
        then // a face in the current group
          let verts = (line.Substring(2, String.length line - 2)).Split(' ')
          let verts = Array.map (fun (v : string) -> v.Split('/')) verts
          let indexes = Array.map (Array.map (fun (s : string) -> Convert.ToInt32(s))) verts
          let face = Array.map (fun v -> (Array.get v 0 - 1, Array.get v 1 - 1, Array.get v 2 - 1)) indexes
          ((groupname,face::faces), groups)
        else // error case - did not strip all other lines
          Console.WriteLine("Error length: " + Convert.ToString(String.length line))
          Console.WriteLine("Error line: " + line)
          raise ExportError

    let (g,gs) = Array.fold groupfolder (("",[]),[]) groups
    let groups = List.toArray (g::gs)

    (vertices, normals, texcoords, groups)

// unique groups become standard triangle meshes
// groups with the same name are treated as instances of the same mesh
type group = (string * ((int*int*int)[] list))
let partitionGroups (groups : group[]) =
    let groups = groups
                 |> Array.filter (fun (name, faces) -> match faces with | [] -> false | f::fs -> true)
                 |> Array.sortBy (fun (name, faces) -> name)
    
    let groupfolder (s : string, g::gs : group list list) ((name, faces) : group) =
        if s.Equals(name) then
          (s, ((name,faces)::g)::gs)
        else
          (name, [(name,faces)]::g::gs)

    if Array.length groups < 2
    then
      (groups, [||])
    else
      let (name, faces) = Array.get groups 0
      let (name, groups) = Array.fold groupfolder (name, [[(name,faces)]]) (Array.sub groups 1 (Array.length groups - 1))
      let (meshes, leaves) = List.toArray groups |> Array.map List.toArray |> Array.partition (fun a -> Array.length a = 1)
      let meshes = Array.map (fun a -> Array.get a 0) meshes                      
      (meshes, leaves)

// all groups become standard triangle meshes
// groups with same name are combined into a single mesh
let mergeGroups (groups : group[]) =
    let groups = groups
                 |> Array.filter (fun (name, faces) -> match faces with | [] -> false | f::fs -> true)
                 |> Array.sortBy (fun (name, faces) -> name)

    let groupfolder ((s,fs) : group, gs : group list) ((name, faces) : group) =
        if s.Equals(name) then
          ((s, faces@fs), gs)
        else
          ((name,faces), (s,fs)::gs)

    if Array.length groups < 2
    then
      groups
    else
      let (g, gs) = Array.fold groupfolder (Array.get groups 0, []) (Array.sub groups 1 (Array.length groups - 1))
      List.toArray (g::gs)

let meshFromGroup vertices normals texcoords (groupname : string, faces : (int * int * int)[] list) =

    // build a vertex buffer, strip duplicate vertices
    let vertcount = ref(0)
    let vertmap = ref(Map.empty)

    // take one face, find or add its vertices to the buffer, get corresponding indexes
    let facefolder (faces : (int*int*int) list) (face : (int*int*int)[]) =
        let addVert ((v,t,n) : int*int*int) =
            let (vx,vy,vz) = Array.get vertices v
            let (nx,ny,nz) = Array.get normals n
            let (tu,tv) = Array.get texcoords t
            match Map.tryFind (vx,vy,vz,nx,ny,nz,tu,tv) (!vertmap) with
            | Some index -> index
            | None -> let index = !vertcount
                      vertcount := index + 1
                      vertmap := Map.add (vx,vy,vz,nx,ny,nz,tu,tv) index (!vertmap)
                      index
        let indexes = Array.map addVert face

        // decompose face into triangles, add to output
        if Array.length indexes = 3
        then
          (Array.get indexes 0, Array.get indexes 1, Array.get indexes 2)::faces
        else if Array.length indexes = 4
        then
          (Array.get indexes 0, Array.get indexes 1, Array.get indexes 2)::(Array.get indexes 0, Array.get indexes 2, Array.get indexes 3)::faces
        else
          Console.WriteLine("Error: only triangle or quad faces supported")
          raise ExportError

    let indexlist = List.toArray (List.fold facefolder [] faces)
    let vertexlist = Map.toArray (!vertmap)
                     |> Array.sortBy (fun (v, i) -> i)
                     |> Array.map (fun (v,i) -> v)

    (groupname, indexlist, vertexlist)

let leafMeshFromGroups vertices normals texcoords (groups : group[]) =

    let rotateZ t (x,y,z) = (x * cos(t) - y * sin(t), x * sin(t) + y * cos(t), z)
    let rotateY t (x,y,z) = (x * cos(t) - z * sin(t), y, x * sin(t) + z * cos(t))
    let rotateX t (x,y,z) = (x, y * cos(t) - z * sin(t), y * sin(t) + z * cos(t))

    let Ns = Array.map (fun (s, f::fs) -> Array.get normals (let (_,_,n) = Array.get f 0 in n)) groups
    let denoms = Array.map (fun (x,y,z) -> sqrt(x*x + z*z)) Ns
    let Thetas = Array.map2 (fun d (x,y,z) -> if d = 0.f then 0.f else -acos(z / d)) denoms Ns
    let Phis = Array.map2 (fun d (x,y,z) -> asin(y / d)) denoms Ns

    let Ts = Array.map (fun (s, f::fs) -> Array.get vertices (let (p,_,_) = Array.get f 0 in p)) groups

    let Deltas = Array.map (fun (s, f::fs) -> Array.get vertices (let (p,_,_) = Array.get f 1 in p)) groups
                 |> Array.map2 (fun (tx,ty,tz) (x,y,z) -> (x - tx, y - ty, z - tz)) Ts
                 |> Array.map2 (fun t (x,y,z) -> rotateZ t (x,y,z)) Thetas
                 |> Array.map2 (fun p (x,y,z) -> rotateY p (x,y,z)) Phis
                 |> Array.map (fun (x,y,z) -> let d = sqrt(x*x + y*y + z*z) in if d = 0.f then 0.f else asin(x / d))

    let instances = Array.init (Array.length groups) (fun i -> let (tx,ty,tz) = Array.get Ts i
                                                               let theta = Array.get Thetas i
                                                               let phi = Array.get Phis i
                                                               let delta = Array.get Deltas i
                                                               (tx,ty,tz,theta,phi,delta))

    let (name, indices, vertices) = meshFromGroup vertices normals texcoords (Array.get groups 0)

    let vertices = let (tx,ty,tz) = Array.get Ts 0
                   let theta = Array.get Thetas 0
                   let phi = Array.get Phis 0
                   let delta = Array.get Deltas 0

                   let baseVertFromFirstInstance (vx,vy,vz,nx,ny,nz,tu,tv) = let (vx',vy',vz') = (vx - tx, vy - ty, vz - tz)
                                                                                                 |> rotateZ theta
                                                                                                 |> rotateY phi
                                                                                                 |> rotateX delta
                                                                             let (nx',ny',nz') = (nx,ny,nz)
                                                                                                 |> rotateZ theta
                                                                                                 |> rotateY phi
                                                                                                 |> rotateX delta
                                                                             (vx',vy',vz',nx',ny',nz',tu,tv)

                   Array.map baseVertFromFirstInstance vertices

    (name, vertices, indices, instances)

let meshToString (name, indexbuf, vertexbuf) =
    let header = sprintf "*MESH %s\n" name

    // materials not yet parsed
    let material = "*MATERIAL 0.1 0.5 0 765.265\n"
    let texture = "*TEXTURE Querus_trunk.tif\n"

    let vertToString (vx : float32, vy : float32, vz : float32, nx : float32, ny : float32, nz : float32, tu : float32, tv : float32) = 
                                                 Convert.ToString(vx) + " " + Convert.ToString(vy) + " " + Convert.ToString(vz) + " "
                                               + Convert.ToString(nx) + " " + Convert.ToString(ny) + " " + Convert.ToString(nz) + " "
                                               + Convert.ToString(tu) + " " + Convert.ToString(tv) + "\n"

    let vertices = (sprintf "*VERTICES %d\n" (Array.length vertexbuf)) + (Array.reduce (fun a b -> a + b) (Array.map vertToString vertexbuf))

    let triToString (a : int, b : int, c : int) = Convert.ToString(a) + " " + Convert.ToString(b) + " " + Convert.ToString(c) + " "

    let indices = (sprintf "*INDICES %d\n" (3 * (Array.length indexbuf))) + (Array.reduce (fun a b -> a + b) (Array.map triToString indexbuf)) + "\n"

    header + material + texture + vertices + indices

let leafMeshToString (name, vertices, indices, instances) =
    let header = sprintf "*LEAFMESH %s\n" name

    // materials not yet parsed
    let material = "*MATERIAL 0.1 0.5 0 765.265\n"
    let texture = "*TEXTURE Querus_leaf.tif\n"

    let vertToString (vx : float32, vy : float32, vz : float32, nx : float32, ny : float32, nz : float32, tu : float32, tv : float32) = 
                                                 Convert.ToString(vx) + " " + Convert.ToString(vy) + " " + Convert.ToString(vz) + " "
                                               + Convert.ToString(nx) + " " + Convert.ToString(ny) + " " + Convert.ToString(nz) + " "
                                               + Convert.ToString(tu) + " " + Convert.ToString(tv) + "\n"

    let vertices = (sprintf "*VERTICES %d\n" (Array.length vertices)) + (Array.reduce (fun a b -> a + b) (Array.map vertToString vertices))

    let triToString (a : int, b : int, c : int) = Convert.ToString(a) + " " + Convert.ToString(b) + " " + Convert.ToString(c) + " "

    let indices = (sprintf "*INDICES %d\n" (3 * (Array.length indices))) + (Array.reduce (fun a b -> a + b) (Array.map triToString indices)) + "\n"

    let instToString (tx : float32, ty : float32, tz : float32, pitch : float32, yaw : float32, roll : float32) =
        Convert.ToString(tx) + " " + Convert.ToString(ty) + " " + Convert.ToString(tz) + " " +
        Convert.ToString(pitch) + " " + Convert.ToString(yaw) + " " + Convert.ToString(roll) + "\n"

    let instances = (sprintf "*INSTANCES %d\n" (Array.length instances)) + (Array.reduce (fun a b -> a + b) (Array.map instToString instances)) + "\n"

    header + material + texture + vertices + indices + instances

let formatMeshes (meshes, leafmeshes) = (Array.reduce (fun a b -> a + b) (Array.map meshToString meshes)) + (Array.reduce (fun a b -> a + b) (Array.map leafMeshToString leafmeshes))

// convert a .obj to a .fmt with instanced leaves
let obj2fmt filename =
    let (v,n,t,g) = parseObj filename
    let (meshes, leaves) = partitionGroups g
    let (meshes, leaves) = (Array.map (meshFromGroup v n t) meshes, Array.map (leafMeshFromGroups v n t) leaves)
    let name = filename.Substring(0, String.length filename - 3) + "fmt"
    File.WriteAllText(name, formatMeshes(meshes, leaves))

// convert a .obj to a .fmt with no instancing
let obj2fmt2 filename =
    let (v,n,t,g) = parseObj filename
    let groups = mergeGroups g
    let meshes = Array.map (meshFromGroup v n t) groups
    let name = filename.Substring(0, String.length filename - 3) + "no_instancing.fmt"
    File.WriteAllText(name, formatMeshes(meshes, [||]))
