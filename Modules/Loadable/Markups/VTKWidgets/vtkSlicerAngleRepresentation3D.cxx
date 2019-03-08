/*=========================================================================

 Copyright (c) ProxSim ltd., Kwun Tong, Hong Kong. All Rights Reserved.

 See COPYRIGHT.txt
 or http://www.slicer.org/copyright/copyright.txt for details.

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 This file was originally developed by Davide Punzo, punzodavide@hotmail.it,
 and development was supported by ProxSim ltd.

=========================================================================*/

#include "vtkSlicerAngleRepresentation3D.h"
#include "vtkCleanPolyData.h"
#include "vtkOpenGLPolyDataMapper.h"
#include "vtkOpenGLActor.h"
#include "vtkActor2D.h"
#include "vtkAssemblyPath.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkObjectFactory.h"
#include "vtkProperty.h"
#include "vtkAssemblyPath.h"
#include "vtkMath.h"
#include "vtkInteractorObserver.h"
#include "vtkLine.h"
#include "vtkCoordinate.h"
#include "vtkGlyph3D.h"
#include "vtkCursor2D.h"
#include "vtkCylinderSource.h"
#include "vtkPolyData.h"
#include "vtkPoints.h"
#include "vtkDoubleArray.h"
#include "vtkPointData.h"
#include "vtkTransformPolyDataFilter.h"
#include "vtkTransform.h"
#include "vtkCamera.h"
#include "vtkPoints.h"
#include "vtkCellArray.h"
#include "vtkSphereSource.h"
#include "vtkPropPicker.h"
#include "vtkPickingManager.h"
#include "vtkAppendPolyData.h"
#include "vtkStringArray.h"
#include "vtkTubeFilter.h"
#include "vtkOpenGLTextActor.h"
#include "cmath"
#include "vtkArcSource.h"
#include "vtkTextProperty.h"
#include "vtkMRMLMarkupsDisplayNode.h"

vtkStandardNewMacro(vtkSlicerAngleRepresentation3D);

//----------------------------------------------------------------------
vtkSlicerAngleRepresentation3D::vtkSlicerAngleRepresentation3D()
{
  this->Line = vtkSmartPointer<vtkPolyData>::New();
  this->Arc = vtkSmartPointer<vtkArcSource>::New();
  this->Arc->SetResolution(30);

  this->TubeFilter = vtkSmartPointer<vtkTubeFilter>::New();
  this->TubeFilter->SetInputData(this->Line);
  this->TubeFilter->SetNumberOfSides(20);
  this->TubeFilter->SetRadius(1);

  this->ArcTubeFilter = vtkSmartPointer<vtkTubeFilter>::New();
  this->ArcTubeFilter->SetInputConnection(this->Arc->GetOutputPort());
  this->ArcTubeFilter->SetNumberOfSides(20);
  this->ArcTubeFilter->SetRadius(1);

  this->LineMapper = vtkSmartPointer<vtkOpenGLPolyDataMapper>::New();
  this->LineMapper->SetInputConnection(this->TubeFilter->GetOutputPort());

  this->ArcMapper = vtkSmartPointer<vtkOpenGLPolyDataMapper>::New();
  this->ArcMapper->SetInputConnection(this->ArcTubeFilter->GetOutputPort());

  this->LineActor = vtkSmartPointer<vtkOpenGLActor>::New();
  this->LineActor->SetMapper(this->LineMapper);
  this->LineActor->SetProperty(this->GetControlPointsPipeline(Unselected)->Property);

  this->ArcActor = vtkSmartPointer<vtkOpenGLActor>::New();
  this->ArcActor->SetMapper(this->ArcMapper);
  this->ArcActor->SetProperty(this->GetControlPointsPipeline(Unselected)->Property);

  this->TextActor = vtkSmartPointer<vtkOpenGLTextActor>::New();
  this->TextActor->SetInput("0");
  this->TextActor->SetTextProperty(this->GetControlPointsPipeline(Unselected)->TextProperty);

  this->LabelFormat = "%-#6.3g";

  //Manage the picking
  this->LinePicker = vtkSmartPointer<vtkPropPicker>::New();
  this->LinePicker->PickFromListOn();
  this->LinePicker->InitializePickList();
  this->LinePicker->AddPickList(this->LineActor);
  this->LinePicker->AddPickList(this->ArcActor);
}

//----------------------------------------------------------------------
vtkSlicerAngleRepresentation3D::~vtkSlicerAngleRepresentation3D()
{
}

//----------------------------------------------------------------------
bool vtkSlicerAngleRepresentation3D::GetTransformationReferencePoint(double referencePointWorld[3])
{
  vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
  if (!markupsNode || markupsNode->GetNumberOfControlPoints() < 2)
    {
    return false;
    }
  markupsNode->GetNthControlPointPositionWorld(1, referencePointWorld);
  return true;
}

//----------------------------------------------------------------------
void vtkSlicerAngleRepresentation3D::BuildArc()
{
  vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
  if (!markupsNode)
    {
    return;
    }

  // Build Arc
  if (markupsNode->GetNumberOfControlPoints() != 3)
    {
    return;
    }

  double p1[3], p2[3], c[3], vector2[3], vector1[3];
  double l1 = 0.0, l2 = 0.0;
  markupsNode->GetNthControlPointPositionWorld(0, p1);
  markupsNode->GetNthControlPointPositionWorld(1, c);
  markupsNode->GetNthControlPointPositionWorld(2, p2);

  // Compute the angle (only if necessary since we don't want
  // fluctuations in angle value as the camera moves, etc.)
  if (fabs(p1[0]-c[0]) < 0.001 || fabs(p2[0]-c[0]) < 0.001)
    {
    return;
    }

  vector1[0] = p1[0] - c[0];
  vector1[1] = p1[1] - c[1];
  vector1[2] = p1[2] - c[2];
  vector2[0] = p2[0] - c[0];
  vector2[1] = p2[1] - c[1];
  vector2[2] = p2[2] - c[2];
  l1 = vtkMath::Normalize(vector1);
  l2 = vtkMath::Normalize(vector2);
  double angle = acos(vtkMath::Dot(vector1, vector2));

  // Place the label and place the arc
  const double length = l1 < l2 ? l1 : l2;
  const double anglePlacementRatio = 0.5;
  const double l = length * anglePlacementRatio;
  double arcp1[3] = {l * vector1[0] + c[0],
                     l * vector1[1] + c[1],
                     l * vector1[2] + c[2]};
  double arcp2[3] = {l * vector2[0] + c[0],
                     l * vector2[1] + c[1],
                     l * vector2[2] + c[2]};

  this->Arc->SetPoint1(arcp1);
  this->Arc->SetPoint2(arcp2);
  this->Arc->SetCenter(c);
  this->Arc->Update();

  char string[512];
  snprintf(string, sizeof(string), this->LabelFormat.c_str(), vtkMath::DegreesFromRadians(angle));
  this->TextActor->SetInput(string);

  double textPos[3], vector3[3];
  vector3[0] = vector1[0] + vector2[0];
  vector3[1] = vector1[1] + vector2[1];
  vector3[2] = vector1[2] + vector2[2];
  vtkMath::Normalize(vector3);
  textPos[0] = c[0] + vector3[0] * length * 0.6;
  textPos[1] = c[1] + vector3[1] * length * 0.6;
  textPos[2] = c[2] + vector3[2] * length * 0.6;
  this->Renderer->SetWorldPoint(textPos);
  this->Renderer->WorldToDisplay();
  this->Renderer->GetDisplayPoint(textPos);

  int X = static_cast<int>(textPos[0]);
  int Y = static_cast<int>(textPos[1]);
  this->TextActor->SetDisplayPosition(X,Y);
}

//----------------------------------------------------------------------
void vtkSlicerAngleRepresentation3D::UpdateFromMRML(vtkMRMLNode* caller, unsigned long event, void *callData /*=NULL*/)
{
  Superclass::UpdateFromMRML(caller, event, callData);

  this->NeedToRenderOn();

  vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
  if (!markupsNode || !this->MarkupsDisplayNode
    || !this->MarkupsDisplayNode->GetVisibility()
    || !this->MarkupsDisplayNode->IsDisplayableInView(this->ViewNode->GetID())
    )
  {
    this->VisibilityOff();
    return;
  }

  this->VisibilityOn();

  // Update lines geometry

  this->BuildLine(this->Line, false);
  this->BuildArc();

  // Update lines display properties

  for (int controlPointType = 0; controlPointType < NumberOfControlPointTypes; ++controlPointType)
    {
    ControlPointsPipeline3D* controlPoints = this->GetControlPointsPipeline(controlPointType);
    controlPoints->LabelsActor->SetVisibility(this->MarkupsDisplayNode->GetTextVisibility());
    controlPoints->Glypher->SetScaleFactor(this->ControlPointSize);
    this->UpdateRelativeCoincidentTopologyOffsets(controlPoints->Mapper);
    }

  this->TextActor->SetVisibility(this->MarkupsDisplayNode->GetTextVisibility());

  this->UpdateRelativeCoincidentTopologyOffsets(this->LineMapper);
  this->UpdateRelativeCoincidentTopologyOffsets(this->ArcMapper);

  this->TubeFilter->SetRadius(this->ControlPointSize * 0.125);
  this->ArcTubeFilter->SetRadius(this->ControlPointSize * 0.125);

  bool lineVisibility = this->GetAllControlPointsVisible();

  this->LineActor->SetVisibility(lineVisibility);
  this->ArcActor->SetVisibility(lineVisibility && markupsNode->GetNumberOfControlPoints() == 3);
  this->TextActor->SetVisibility(lineVisibility && markupsNode->GetNumberOfControlPoints() == 3);

  int controlPointType = Active;
  if (this->MarkupsDisplayNode->GetActiveComponentType() != vtkMRMLMarkupsDisplayNode::ComponentLine)
    {
    controlPointType = this->GetAllControlPointsSelected() ? Selected : Unselected;
    }
  this->LineActor->SetProperty(this->GetControlPointsPipeline(controlPointType)->Property);
  this->ArcActor->SetProperty(this->GetControlPointsPipeline(controlPointType)->Property);
  this->TextActor->SetTextProperty(this->GetControlPointsPipeline(controlPointType)->TextProperty);
}

//----------------------------------------------------------------------
void vtkSlicerAngleRepresentation3D::GetActors(vtkPropCollection *pc)
{
  this->Superclass::GetActors(pc);
  this->LineActor->GetActors(pc);
  this->ArcActor->GetActors(pc);
  this->TextActor->GetActors(pc);
}

//----------------------------------------------------------------------
void vtkSlicerAngleRepresentation3D::ReleaseGraphicsResources(
  vtkWindow *win)
{
  this->Superclass::ReleaseGraphicsResources(win);
  this->LineActor->ReleaseGraphicsResources(win);
  this->ArcActor->ReleaseGraphicsResources(win);
  this->TextActor->ReleaseGraphicsResources(win);
}

//----------------------------------------------------------------------
int vtkSlicerAngleRepresentation3D::RenderOverlay(vtkViewport *viewport)
{
  int count=0;
  count = this->Superclass::RenderOverlay(viewport);
  if (this->LineActor->GetVisibility())
    {
    count +=  this->LineActor->RenderOverlay(viewport);
    }
  if (this->ArcActor->GetVisibility())
    {
    count +=  this->ArcActor->RenderOverlay(viewport);
    }
  if (this->TextActor->GetVisibility())
    {
    count +=  this->TextActor->RenderOverlay(viewport);
    }
  return count;
}

//-----------------------------------------------------------------------------
int vtkSlicerAngleRepresentation3D::RenderOpaqueGeometry(
  vtkViewport *viewport)
{
  int count=0;
  count = this->Superclass::RenderOpaqueGeometry(viewport);
  if (this->LineActor->GetVisibility())
    {
    count += this->LineActor->RenderOpaqueGeometry(viewport);
    }
  if (this->ArcActor->GetVisibility())
    {
    count += this->ArcActor->RenderOpaqueGeometry(viewport);
    }
  if (this->TextActor->GetVisibility())
    {
    count += this->TextActor->RenderOpaqueGeometry(viewport);
    }
  return count;
}

//-----------------------------------------------------------------------------
int vtkSlicerAngleRepresentation3D::RenderTranslucentPolygonalGeometry(
  vtkViewport *viewport)
{
  int count=0;
  count = this->Superclass::RenderTranslucentPolygonalGeometry(viewport);
  if (this->LineActor->GetVisibility())
    {
    count += this->LineActor->RenderTranslucentPolygonalGeometry(viewport);
    }
  if (this->ArcActor->GetVisibility())
    {
    count += this->ArcActor->RenderTranslucentPolygonalGeometry(viewport);
    }
  if (this->TextActor->GetVisibility())
    {
    count += this->TextActor->RenderTranslucentPolygonalGeometry(viewport);
    }
  return count;
}

//-----------------------------------------------------------------------------
vtkTypeBool vtkSlicerAngleRepresentation3D::HasTranslucentPolygonalGeometry()
{
  int result=0;
  result |= this->Superclass::HasTranslucentPolygonalGeometry();
  if (this->LineActor->GetVisibility())
    {
    result |= this->LineActor->HasTranslucentPolygonalGeometry();
    }
  if (this->ArcActor->GetVisibility())
    {
    result |= this->ArcActor->HasTranslucentPolygonalGeometry();
    }
  if (this->TextActor->GetVisibility())
    {
    result |= this->TextActor->HasTranslucentPolygonalGeometry();
    }
  return result;
}

//----------------------------------------------------------------------
double *vtkSlicerAngleRepresentation3D::GetBounds()
{
  vtkBoundingBox boundingBox;
  const std::vector<vtkProp*> actors({ this->LineActor, this->ArcActor });
  this->AddActorsBounds(boundingBox, actors, Superclass::GetBounds());
  boundingBox.GetBounds(this->Bounds);
  return this->Bounds;
}

//----------------------------------------------------------------------
void vtkSlicerAngleRepresentation3D::CanInteract(
  const int displayPosition[2], const double worldPosition[3],
  int &foundComponentType, int &foundComponentIndex, double &closestDistance2)
{
  foundComponentType = vtkMRMLMarkupsDisplayNode::ComponentNone;
  vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
  if (!markupsNode || markupsNode->GetLocked() || markupsNode->GetNumberOfControlPoints() < 1)
  {
    return;
  }
  Superclass::CanInteract(displayPosition, worldPosition, foundComponentType, foundComponentIndex, closestDistance2);
  if (foundComponentType != vtkMRMLMarkupsDisplayNode::ComponentNone)
  {
    return;
  }

  this->CanInteractWithLine(displayPosition, worldPosition, foundComponentType, foundComponentIndex, closestDistance2);
}


//-----------------------------------------------------------------------------
void vtkSlicerAngleRepresentation3D::PrintSelf(ostream& os, vtkIndent indent)
{
  //Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
  this->Superclass::PrintSelf(os, indent);

  if (this->LineActor)
    {
    os << indent << "Line Visibility: " << this->LineActor->GetVisibility() << "\n";
    }
  else
    {
    os << indent << "Line Visibility: (none)\n";
    }

  if (this->ArcActor)
    {
    os << indent << "Arc Visibility: " << this->ArcActor->GetVisibility() << "\n";
    }
  else
    {
    os << indent << "Arc Visibility: (none)\n";
    }

  if (this->TextActor)
    {
    os << indent << "Text Visibility: " << this->TextActor->GetVisibility() << "\n";
    }
  else
    {
    os << indent << "Text Visibility: (none)\n";
    }

  os << indent << "Label Format: ";
  os << this->LabelFormat << "\n";
}