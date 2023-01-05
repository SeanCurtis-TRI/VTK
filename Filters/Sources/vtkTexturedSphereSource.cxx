/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkTexturedSphereSource.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkTexturedSphereSource.h"

#include "vtkCellArray.h"
#include "vtkFloatArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkTexturedSphereSource);

// Construct sphere with radius=0.5 and default resolution 8 in both Phi
// and Theta directions.
vtkTexturedSphereSource::vtkTexturedSphereSource(int res)
{
  res = res < 4 ? 4 : res;
  this->Radius = 0.5;
  this->ThetaResolution = res;
  this->PhiResolution = res;
  this->Theta = 0.0;
  this->Phi = 0.0;
  this->OutputPointsPrecision = SINGLE_PRECISION;

  this->SetNumberOfInputPorts(0);
}

int vtkTexturedSphereSource::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector), vtkInformationVector* outputVector)
{
  // get the info object
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  // get the output
  vtkPolyData* output = vtkPolyData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  int i, j;
  int numPts;
  int numPolys;
  vtkPoints* newPoints;
  vtkFloatArray* newNormals;
  vtkFloatArray* newTCoords;
  vtkCellArray* newPolys;
  double x[3], deltaPhi, deltaTheta, phi, theta, radius, norm;
  vtkIdType pts[3];
  double tc[2];

  //
  // Set things up; allocate memory
  //

  // TODO: Instead of (phi_R + 1) it should be (phi_R + 2) (two equators).
  numPts = (this->PhiResolution + 1) * (this->ThetaResolution + 1);
  // TODO: Num of polys increase by theta_R * 2 (i.e., one more band of tris
  // around the barrel of the cylinder). Or, (phi_R + 1) * 2 * theta_R.
  // creating triangles
  numPolys = this->PhiResolution * 2 * this->ThetaResolution;

  newPoints = vtkPoints::New();

  // Set the desired precision for the points in the output.
  if (this->OutputPointsPrecision == vtkAlgorithm::DOUBLE_PRECISION)
  {
    newPoints->SetDataType(VTK_DOUBLE);
  }
  else
  {
    newPoints->SetDataType(VTK_FLOAT);
  }

  newPoints->Allocate(numPts);
  newNormals = vtkFloatArray::New();
  newNormals->SetNumberOfComponents(3);
  newNormals->Allocate(3 * numPts);
  newTCoords = vtkFloatArray::New();
  newTCoords->SetNumberOfComponents(2);
  newTCoords->Allocate(2 * numPts);
  newPolys = vtkCellArray::New();
  newPolys->AllocateEstimate(numPolys, 3);
  //
  // Create sphere
  //
  // Create intermediate points
  // TODO: We need to make sure that j * deltaPhi (for some integer j) is equal
  // to pi/2. Hard requirement. Generally, not true for arbitrary PhiResolution.
  // Define *temporary* PhiResolution:
  //  PhiResolutionHalf = PhiResolution / 2.
  //  deltaPhi = (vtkMath::pi() / 2) / PhiResolutionHalf.
  // The for loop now uses PhiResolutionHalf instead of PhiResolution.
  //  - the second for loop would actually go from PhiResolutionHalf to 2 * PhiResolutionHalf (so that j * phi works out).
  deltaPhi = vtkMath::Pi() / this->PhiResolution;
  deltaTheta = 2.0 * vtkMath::Pi() / this->ThetaResolution;
  for (i = 0; i <= this->ThetaResolution; i++)
  {
    theta = i * deltaTheta;
    tc[0] = theta / (2.0 * vtkMath::Pi());
    // TODO:
    //  1. Loop runs j = 0; <= phi_R / 2.
    //  2. Start *again* j = phi_R / 2; j <= phi_R.
    for (j = 0; j <= this->PhiResolution; j++)
    {
      phi = j * deltaPhi;
      radius = this->Radius * sin((double)phi);
      // The capsule is oriented along the y-axis; this sphere is oriented
      // along the z-axis. So, swap x[1] and x[2] and add +/-L/2 to x[1].
      x[0] = radius * cos((double)theta);
      x[1] = radius * sin((double)theta);
      x[2] = this->Radius * cos((double)phi);
      newPoints->InsertNextPoint(x);

      if ((norm = vtkMath::Norm(x)) == 0.0)
      {
        norm = 1.0;
      }
      x[0] /= norm;
      x[1] /= norm;
      x[2] /= norm;
      newNormals->InsertNextTuple(x);

      // TODO: Different!
      //  - For northern hemisphere: (ϕR) / (πR + L)
      //  - For southern hemisphere: [L + ϕR] / (πR + L)
      //  Possibly one minus those quantities if we need to flip the image
      //  vertically.
      tc[1] = 1.0 - phi / vtkMath::Pi();  // 1 - x means flipped image.
      newTCoords->InsertNextTuple(tc);
    }
  }
  // Generate mesh connectivity
  //
  // bands between poles
  for (i = 0; i < this->ThetaResolution; i++)
  {
    // New bounds are j = 0; j < 2 * PhiResolutionHalf; ++j.
    for (j = 0; j < this->PhiResolution; j++)
    {
      pts[0] = (this->PhiResolution + 1) * i + j;
      pts[1] = pts[0] + 1;
      pts[2] = ((this->PhiResolution + 1) * (i + 1) + j) + 1;
      newPolys->InsertNextCell(3, pts);

      pts[1] = pts[2];
      pts[2] = pts[1] - 1;
      newPolys->InsertNextCell(3, pts);
    }
  }
  //
  // Update ourselves and release memory
  //
  output->SetPoints(newPoints);
  newPoints->Delete();

  output->GetPointData()->SetNormals(newNormals);
  newNormals->Delete();

  output->GetPointData()->SetTCoords(newTCoords);
  newTCoords->Delete();

  output->SetPolys(newPolys);
  newPolys->Delete();

  return 1;
}

void vtkTexturedSphereSource::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Theta Resolution: " << this->ThetaResolution << "\n";
  os << indent << "Phi Resolution: " << this->PhiResolution << "\n";
  os << indent << "Theta: " << this->Theta << "\n";
  os << indent << "Phi: " << this->Phi << "\n";
  os << indent << "Radius: " << this->Radius << "\n";
  os << indent << "Output Points Precision: " << this->OutputPointsPrecision << "\n";
}
VTK_ABI_NAMESPACE_END
