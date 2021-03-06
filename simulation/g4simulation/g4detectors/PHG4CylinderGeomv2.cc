#include "PHG4CylinderGeomv2.h"
#include "PHG4Parameters.h"

ClassImp(PHG4CylinderGeomv2)

using namespace std;

PHG4CylinderGeomv2::PHG4CylinderGeomv2():
  nscint(-9999)
{
  return;
}

void
PHG4CylinderGeomv2::identify(std::ostream& os) const
{
  os << "PHG4CylinderGeomv2: layer: " << layer 
     << ", radius: " << radius 
     << ", thickness: " << thickness
     << ", zmin: " << zmin 
     << ", zmax: " << zmax 
     << ", num scint: " << nscint
     << endl;
  return;
}

void
PHG4CylinderGeomv2::ImportParameters(const PHG4Parameters & param)
{
  PHG4CylinderGeomv1::ImportParameters(param);

  if (param.exist_int_param("nscint")) nscint = param.get_int_param("nscint");

  return;
}
