// Copyright (c) 2009-2017 The Regents of the University of Michigan
// This file is part of the HOOMD-blue project, released under the BSD 3-Clause License.

// Include the defined classes that are to be exported to python
#include "IntegratorHPMC.h"
#include "IntegratorHPMCMono.h"
#include "IntegratorHPMCMonoImplicit.h"
#include "ComputeFreeVolume.h"

#include "ShapeSphere.h"
#include "ShapeConvexPolygon.h"
#include "ShapePolyhedron.h"
#include "ShapeConvexPolyhedron.h"
#include "ShapeSpheropolyhedron.h"
#include "ShapeSpheropolygon.h"
#include "ShapeSimplePolygon.h"
#include "ShapeEllipsoid.h"
#include "ShapeFacetedSphere.h"
#include "ShapeSphinx.h"
#include "AnalyzerSDF.h"
#include "ShapeUnion.h"

#include "ExternalField.h"
#include "ExternalFieldWall.h"
#include "ExternalFieldLattice.h"
#include "ExternalFieldComposite.h"

#include "UpdaterExternalFieldWall.h"
#include "UpdaterRemoveDrift.h"
#include "UpdaterMuVT.h"
#include "UpdaterMuVTImplicit.h"

#ifdef ENABLE_CUDA
#include "IntegratorHPMCMonoGPU.h"
#include "IntegratorHPMCMonoImplicitGPU.h"
#include "ComputeFreeVolumeGPU.h"
#endif

namespace py = pybind11;
using namespace hpmc;

using namespace hpmc::detail;

namespace hpmc
{

//! Export the base HPMCMono integrators
void export_ellipsoid(py::module& m)
    {
    export_IntegratorHPMCMono< ShapeEllipsoid >(m, "IntegratorHPMCMonoEllipsoid");
    export_IntegratorHPMCMonoImplicit< ShapeEllipsoid >(m, "IntegratorHPMCMonoImplicitEllipsoid");
    export_ComputeFreeVolume< ShapeEllipsoid >(m, "ComputeFreeVolumeEllipsoid");
    export_AnalyzerSDF< ShapeEllipsoid >(m, "AnalyzerSDFEllipsoid");
    export_UpdaterMuVT< ShapeEllipsoid >(m, "UpdaterMuVTEllipsoid");
    export_UpdaterMuVTImplicit< ShapeEllipsoid >(m, "UpdaterMuVTImplicitEllipsoid");

    export_ExternalFieldInterface<ShapeEllipsoid>(m, "ExternalFieldEllipsoid");
    export_LatticeField<ShapeEllipsoid>(m, "ExternalFieldLatticeEllipsoid");
    export_ExternalFieldComposite<ShapeEllipsoid>(m, "ExternalFieldCompositeEllipsoid");
    export_RemoveDriftUpdater<ShapeEllipsoid>(m, "RemoveDriftUpdaterEllipsoid");
    export_ExternalFieldWall<ShapeEllipsoid>(m, "WallEllipsoid");
    export_UpdaterExternalFieldWall<ShapeEllipsoid>(m, "UpdaterExternalFieldWallEllipsoid");

    #ifdef ENABLE_CUDA
    export_IntegratorHPMCMonoGPU< ShapeEllipsoid >(m, "IntegratorHPMCMonoGPUEllipsoid");
    export_IntegratorHPMCMonoImplicitGPU< ShapeEllipsoid >(m, "IntegratorHPMCMonoImplicitGPUEllipsoid");
    export_ComputeFreeVolumeGPU< ShapeEllipsoid >(m, "ComputeFreeVolumeGPUEllipsoid");
    #endif
    }

}
