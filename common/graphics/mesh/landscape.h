#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include "graphics/visualobjects.h"
#include "graphics/mesh/submesh/staticmesh.h"

class PackedMesh;

class Landscape final {
  public:
    Landscape(VisualObjects& visual, const PackedMesh& wmesh);

    const Tempest::StorageBuffer& bvh()   const { return bvhNodes; }

  private:
    using Item = VisualObjects::Item;

    struct Block {
      Item mesh;
      };

    // Diathic: mesh before blocks - members are destroyed in reverse order, and freeing a block reads the mesh
    // (VisualObjects::free -> updateRtAs -> StaticMesh::blas); the other way round = crash on loading a save with RT
    StaticMesh             mesh;
    std::vector<Block>     blocks;
    Tempest::StorageBuffer meshletDesc;

    Tempest::StorageBuffer bvhNodes;
  };
