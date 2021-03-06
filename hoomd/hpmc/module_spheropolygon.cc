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
void export_spheropolygon(py::module& m)
    {
    export_IntegratorHPMCMono< ShapeSpheropolygon >(m, "IntegratorHPMCMonoSpheropolygon");
    export_IntegratorHPMCMonoImplicit< ShapeSpheropolygon >(m, "IntegratorHPMCMonoImplicitSpheropolygon");
    export_ComputeFreeVolume< ShapeSpheropolygon >(m, "ComputeFreeVolumeSpheropolygon");
    export_AnalyzerSDF< ShapeSpheropolygon >(m, "AnalyzerSDFSpheropolygon");
    export_UpdaterMuVT< ShapeSpheropolygon >(m, "UpdaterMuVTSpheropolygon");
    export_UpdaterMuVTImplicit< ShapeSpheropolygon >(m, "UpdaterMuVTImplicitSpheropolygon");

    export_ExternalFieldInterface<ShapeSpheropolygon>(m, "ExternalFieldSpheropolygon");
    export_LatticeField<ShapeSpheropolygon>(m, "ExternalFieldLatticeSpheropolygon");
    export_ExternalFieldComposite<ShapeSpheropolygon>(m, "ExternalFieldCompositeSpheropolygon");
    export_RemoveDriftUpdater<ShapeSpheropolygon>(m, "RemoveDriftUpdaterSpheropolygon");
    // export_ExternalFieldWall<ShapeSpheropolygon>(m, "WallSpheropolygon");
    // export_UpdaterExternalFieldWall<ShapeSpheropolygon>(m, "UpdaterExternalFieldWallSpheropolygon");

    #ifdef ENABLE_CUDA
    export_IntegratorHPMCMonoGPU< ShapeSpheropolygon >(m, "IntegratorHPMCMonoGPUSpheropolygon");
    export_IntegratorHPMCMonoImplicitGPU< ShapeSpheropolygon >(m, "IntegratorHPMCMonoImplicitGPUSpheropolygon");
    export_ComputeFreeVolumeGPU< ShapeSpheropolygon >(m, "ComputeFreeVolumeGPUSpheropolygon");
    #endif
    }

}
