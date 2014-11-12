#include "stdafx.h"
#include "Map.h"
#include "Measuring.h"
#include "MapTracker.h"
#include "ShapeEditor.h"
#include "Shapefile.h"
#include "ShapefileHelper.h"


// ************************************************************
//		ParseKeyboardEventFlags
// ************************************************************
long CMapView::ParseKeyboardEventFlags(UINT nFlags)
{
	long vbflags = 0;
	if (nFlags & MK_SHIFT)
		vbflags |= 1;
	if (nFlags & MK_CONTROL)
		vbflags |= 2;
	return vbflags;
}

// ************************************************************
//		ParseMouseEventFlags
// ************************************************************
long CMapView::ParseMouseEventFlags(UINT nFlags)
{
	long mbutton = 0;
	if (nFlags & MK_LBUTTON)
		mbutton = 1;
	else if (nFlags & MK_RBUTTON)
		mbutton = 2;
	else if (nFlags & MK_MBUTTON)
		mbutton = 3;
	return mbutton;
}

#pragma region Keyboard events
// ***************************************************************
//		OnKeyUp
// ***************************************************************
void CMapView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch(nChar)
	{
		case VK_SPACE:
			if (_spacePressed)
			{
				TurnOffPanning();
				_spacePressed = false;
				Debug::WriteWithTime("Space up");
			}
			break;
	}
}

// ***************************************************************
//		TurnOffPanning
// ***************************************************************
void CMapView::TurnOffPanning()
{
	if (m_cursorMode == cmPan && _lastCursorMode != cmNone)
	{
		UpdateCursor(_lastCursorMode, false);
		_lastCursorMode = cmNone;

		if (m_cursorMode == cmMeasure)
		{
			_measuring->put_Persistent(_measuringPersistent ? VARIANT_TRUE: VARIANT_FALSE);
		}

		if (!_panningAnimation)
		{
			// releasing capture for panning operation
			ReleaseCapture();

			//this is the only mode we care about for this event
			_dragging.Start = CPoint(0,0);
			_dragging.Move = CPoint(0,0);

			this->SetExtentsCore(this->_extents, false);
		}
	}
}

// ***************************************************************
//		OnKeyDown
// ***************************************************************
void CMapView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{ 
	double dx = (this->_extents.right - this->_extents.left)/4.0;
	double dy = (this->_extents.top - this->_extents.bottom)/4.0;
	
	CComPtr<IExtents> box = NULL;
	bool arrows = nChar == VK_LEFT || nChar == VK_RIGHT || nChar == VK_UP || nChar == VK_DOWN;
	if (arrows)
		GetUtils()->CreateInstance(idExtents, (IDispatch**)&box);
	
	bool ctrl = GetKeyState(VK_CONTROL) & 0x8000 ? true: false;
	bool shift = GetKeyState(VK_SHIFT) & 0x8000 ? true : false;

	switch(nChar)
	{
		case VK_DELETE:
			{
				if (m_cursorMode == cmEditShape)
				{
					if (_shapeEditor->HandleDelete()) {
						RedrawCore(RedrawSkipDataLayers, false, true);
					}
				}
			}
			break;
		case VK_SPACE:
			{
				bool repetitive =  (nFlags & (1 << 14)) ? true : false;
				if (!repetitive)
				{
					if (m_cursorMode != cmPan)
					{
						Debug::WriteWithTime("Space down: start panning");

						// starting panning
						if (m_cursorMode == cmMeasure)
						{
							VARIANT_BOOL vb;
							_measuring->get_Persistent(&vb);
							_measuringPersistent = vb ? true: false;
							_measuring->put_Persistent(VARIANT_TRUE);
						}
						_lastCursorMode = (tkCursorMode)m_cursorMode;
						UpdateCursor(cmPan, false);
					}
					else
					{
						Debug::WriteWithTime("Space down: turn off panning");
						TurnOffPanning();
					}
				}
				else
				{
  					Debug::WriteWithTime("Holding space");
					_spacePressed = true;
				}
			}
			break;
		case 'P':
			UpdateCursor(cmPan, false);
			break;
		case 'Z':
			{
				if(ctrl) 
				{
					tkUndoShortcut shortcut;
					_undoList->get_ShortcutKey(&shortcut);
					if (shortcut == usCtrlZ) 
					{
						VARIANT_BOOL vb;
						if (shift) {
							_undoList->Redo(VARIANT_TRUE, &vb);
						}
						else {
							_undoList->Undo(VARIANT_TRUE, &vb);
						}
						if (vb) {
							Redraw();    // TODO: run layers redraw only when necessary
						}
						return;
					}
				}
				UpdateCursor(cmZoomIn, false);
			}
			break;
		case 'M':
			if (m_cursorMode == cmMeasure)
			{
				_measuring->Clear();
				tkMeasuringType type;
				_measuring->get_MeasuringType(&type);
				tkMeasuringType newType = type == MeasureArea ? MeasureDistance : MeasureArea;
				_measuring->put_MeasuringType(newType);
				Redraw2(RedrawSkipDataLayers);
			}
			else
			{
				UpdateCursor(cmMeasure, false);
			}
			break;
		case VK_BACK:
			ZoomToPrev();
			break;
		case VK_ADD:
			ZoomIn(0.3);
			break;
		case VK_SUBTRACT:
			ZoomOut(0.3);
			break;
		case VK_MULTIPLY:
			int zoom;
			_tiles->get_CurrentZoom(&zoom);
			ZoomToTileLevel(zoom);
			break;
		case VK_HOME:
			ZoomToMaxExtents();
			break;
		case VK_LEFT:
			if (ctrl) {
				// moving to previous layer
				_activeLayerPosition--;
				if (_activeLayers.size() > 0)
				{
					if (_activeLayerPosition < 0) {
						_activeLayerPosition = _activeLayers.size() - 1;
					}
					int handle = GetLayerHandle(_activeLayerPosition);
					ZoomToLayer(handle);
				}
			}
			else
			{
				box->SetBounds(_extents.left - dx, _extents.bottom, 0.0, _extents.right - dx, _extents.top, 0.0);		
				this->SetExtents(box);
			}
			break;
		case VK_RIGHT:
			if (ctrl) {
				// moving to the next layer
				_activeLayerPosition++;
				if (_activeLayers.size() > 0)
				{
					if (_activeLayerPosition >= (int)_activeLayers.size()) {
						_activeLayerPosition = 0;
					}
					int handle = GetLayerHandle(_activeLayerPosition);
					ZoomToLayer(handle);
				}
			}
			else
			{
				box->SetBounds(_extents.left + dx, _extents.bottom, 0.0, _extents.right + dx, _extents.top, 0.0);		
				this->SetExtents(box);
			}
			break;
		case VK_UP:
			box->SetBounds(_extents.left, _extents.bottom + dy, 0.0, _extents.right, _extents.top + dy, 0.0);		
			this->SetExtents(box);
			break;
		case VK_DOWN:
			box->SetBounds(_extents.left, _extents.bottom - dy, 0.0, _extents.right, _extents.top - dy, 0.0);		
			this->SetExtents(box);
			break;
		default:
			break;
	}
} 
#pragma endregion

#pragma region Mouse wheel
// ***************************************************************
// 		OnMouseWheel()					           
// ***************************************************************
//  Processing mouse wheel event. Amount of zoom is determined by MouseWheelsSpeed parameter
BOOL CMapView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (_mouseWheelSpeed < 0.1 || _mouseWheelSpeed > 10) _mouseWheelSpeed = 1;
	if (_mouseWheelSpeed == 1) return FALSE;
	
	RECT rect;
	double width, height;
	double xCent, yCent;
	double dx, dy;

	// absolute cursor position
	this->GetWindowRect(&rect);
	if (pt.x < rect.left || pt.x > rect.right || pt.y < rect.top || pt.y > rect.bottom)
		return false;
	if ((rect.right - rect.left == 0) && (rect.bottom - rect.top == 0))
		return false;

	if (HasRotation())
	{
		CPoint curMousePt, origMousePt, rotCentre;
	    
		curMousePt.x = pt.x - rect.left;
		curMousePt.y = pt.y - rect.top;
		rotCentre.x = (rect.right - rect.left) / 2;
		rotCentre.y = (rect.bottom - rect.top) / 2;

		_rotate->getOriginalPixelPoint(curMousePt.x, curMousePt.y, &(origMousePt.x), &(origMousePt.y));
		PixelToProj((double)(origMousePt.x), (double)(origMousePt.y), &xCent, &yCent);

		dx = (double)(origMousePt.x) / (double)(rect.right - rect.left);
		dy = (double)(origMousePt.y) / (double)(rect.bottom - rect.top);
	}
	else
	{
		PixelToProj((double)(pt.x - rect.left), (double)(pt.y - rect.top), &xCent, &yCent);
		dx = (double)(pt.x - rect.left) / (rect.right - rect.left);
		dy = (double)(pt.y - rect.top) / (rect.bottom - rect.top);
	}

    // make sure that we have enough momentum to reach the next tile level
	double speed = _mouseWheelSpeed;
	if (ForceDiscreteZoom()) {
		speed = _mouseWheelSpeed > 1 ? 2.001 : 0.499;		// add some margin for rounding error and account for reversed wheeling

		int zoom = GetCurrentZoom();
		int maxZoom, minZoom;
		GetMinMaxZoom(minZoom, maxZoom);
		if (zDelta > 0 && zoom + 1 > maxZoom) {
			return true;
		}
	}
	
	// new extents
	double ratio = zDelta > 0 ? speed : (1/speed);
	height = (_extents.top - _extents.bottom) * ratio;
	width = (_extents.right - _extents.left) * ratio;
	
	Extent ext;
	ext.left = xCent - width * dx;
	ext.right = xCent + width * (1 - dx);
	ext.bottom = yCent - height * (1 - dy);
	ext.top = yCent + height * dy;
	
	SetExtentsCore(ext);

	return true;
}
#pragma endregion

#pragma region Zoombar
// ************************************************************
//		HandleOnZoombarMouseDown
// ************************************************************
bool CMapView::HandleOnZoombarMouseDown( CPoint point )
{
	if (_zoombarVisible && _transformationMode != tmNotDefined)
	{
		int minZoom, maxZoom;
		GetMinMaxZoom(minZoom, maxZoom);
		
		ZoombarPart part = ZoombarHitTest(point.x, point.y);
		switch(part) 
		{
			case ZoombarPart::ZoombarPlus:
				{
					// zoom in
					int zoom = GetCurrentZoom();
					if (zoom <= maxZoom)
					{
						ZoomToTileLevel(zoom + 1);
					}
					return true;
				}

			case ZoombarPart::ZoombarMinus:
				{
					// zoom out
					int zoom = GetCurrentZoom();
					if (zoom - 1 >= minZoom )
					{
						ZoomToTileLevel(zoom - 1);
					}
					return true;
				}

			case ZoombarPart::ZoombarHandle:
				_dragging.Operation = DragZoombarHandle;
				return true;

			case ZoombarPart::ZoombarBar:
				{
					double ratio = _zoombarParts.GetRelativeZoomFromClick(point.y);
					int zoom = (int)(minZoom + (maxZoom - minZoom) * ratio + 0.5);
					ZoomToTileLevel(zoom);
					return true;
				}

			case ZoombarNone: 
			default: 
				return false;
		}
	}
	return false;
}

// ************************************************************
//		HandleOnZoombarMouseMove
// ************************************************************
bool CMapView::HandleOnZoombarMouseMove( CPoint point )
{
	if (_dragging.Operation == DragZoombarHandle)
	{
		RedrawCore(tkRedrawType::RedrawSkipDataLayers, false, true);
		return true;
	}
	else
	{
		ZoombarPart part = ZoombarHitTest(point.x, point.y);
		if (part != _lastZooombarPart)
		{
			_lastZooombarPart = part;		// update before calling OnSetCursor
			OnSetCursor(this,HTCLIENT,0);
			RedrawCore(RedrawSkipDataLayers, false, true);
			_canUseMainBuffer = false;
			this->Refresh();
		}
		return part != ZoombarNone;
	}
}
#pragma endregion

#pragma region Left button

// ************************************************************
//		SelectSingleShape
// ************************************************************
bool CMapView::SelectSingleShape(int x, int y, long& layerHandle, long& shapeIndex)
{
	double projX, projY;
	PixelToProj(x, y, &projX, &projY);

	HotTrackingInfo* info = FindShapeAtScreenPoint(CPoint(x, y), slctInMemorySf);
	if (info)
	{
		layerHandle = info->LayerHandle;
		shapeIndex = info->ShapeId;
		delete info;
		
		tkMwBoolean cancel = blnFalse;
		FireBeforeShapeEdit(uoEditShape, layerHandle, shapeIndex, &cancel);
		return cancel == blnFalse;
	}
	return false;
}

// ************************************************************
//		HandleLButtonDownSelection
// ************************************************************
void CMapView::HandleLButtonDownSelection(CPoint& point, long vbflags)
{
	long x = point.x;
	long y = point.y - 1;

	_ttip.Activate(FALSE);
	CMapTracker selectBox = CMapTracker(this, CRect(0, 0, 0, 0), CRectTracker::solidLine + CRectTracker::resizeOutside);
	selectBox.m_sizeMin = 0;

	bool selected = selectBox.TrackRubberBand(this, point, TRUE) ? true : false;
	_ttip.Activate(TRUE);

	CRect rect = selectBox.m_rect;
	rect.NormalizeRect();

	if ((rect.BottomRight().x - rect.TopLeft().x) < 10 &&
		(rect.BottomRight().y - rect.TopLeft().y) < 10)
		selected = false;

	if (!selected || !m_sendSelectBoxFinal)
	{
		if (m_sendMouseDown == TRUE)
			this->FireMouseDown(MK_LBUTTON, (short)vbflags, x, y);
	}

	if (selected)
	{
		if (HasRotation())
		{
			CRect rectTmp = rect;
			long tmpX = 0, tmpY = 0;
			// adjust rectangle to unrotated coordinates
			_rotate->getOriginalPixelPoint(rect.left, rect.top, &tmpX, &tmpY);
			rectTmp.TopLeft().x = tmpX;
			rectTmp.TopLeft().y = tmpY;
			_rotate->getOriginalPixelPoint(rect.right, rect.bottom, &tmpX, &tmpY);
			rectTmp.BottomRight().x = tmpX;
			rectTmp.BottomRight().y = tmpY;
			rect = rectTmp;
		}

		if (m_sendSelectBoxFinal == TRUE)
		{
			long iby = rect.BottomRight().y;
			long ity = rect.TopLeft().y;
			this->FireSelectBoxFinal(rect.TopLeft().x, rect.BottomRight().x, iby, ity);
			return; // exit out so that the FireMouseUp does not get called! DB 12/10/2002
		}
	}

	//the MapTracker interferes with the OnMouseUp event so we will call it manually
	if (m_sendMouseUp == TRUE)
		this->FireMouseUp(MK_LBUTTON, (short)vbflags, x, y);
}

// ************************************************************
//		OnLButtonDown
// ************************************************************
void CMapView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (HasRotation()) {
		_rotate->getOriginalPixelPoint(point.x, point.y, &(point.x), &(point.y));
	}
	_dragging.Start = point;
	_dragging.Move = point;
	_clickDownExtents = _extents;
	_leftButtonDown = TRUE;

	// process zoombar before everything else; if zoom bar was clicked, 
	// map must not receive the event at all
	if (HandleOnZoombarMouseDown(point))
		return;

	bool ctrl = nFlags & MK_CONTROL ? true: false;

	long vbflags = ParseKeyboardEventFlags(nFlags);

	long x = point.x;
	long y = point.y - 1;

	// --------------------------------------------
	//  Snapping
	// --------------------------------------------
	double projX;
	double projY;
	bool shift = (nFlags & MK_SHIFT) != 0;

	tkSnapBehavior behavior;
	bool snapping = (SnappingIsOn(nFlags, behavior) && IsDigitizingCursor())
					|| (m_cursorMode == cmMeasure && shift);

	VARIANT_BOOL snapped = VARIANT_FALSE;
	if (snapping)
	{
		snapped = FindSnapPoint(GetMouseTolerance(ToleranceSnap, false), point.x, point.y, &projX, &projY);
		if (!snapped && (behavior == sbSnapWithShift || m_cursorMode == cmMeasure) && shift){
			return;  // can't proceed in this mode without snapping
		}
	}
	
	if (!snapped)
		this->PixelToProjection(x, y, projX, projY);

	switch(m_cursorMode)
	{
		case cmZoomIn:
		case cmZoomOut:
		case cmPan:
			if (m_sendMouseDown)
				this->FireMouseDown(MK_LBUTTON, (short)vbflags, x, y);
			break;
	}

	if (_IsSubjectCursor())
	{
		HandleLButtonSubjectCursor(x, y, projX, projY, ctrl);
		return;
	}

	if (IsOverlayCursor())
	{
		tkShapeEditorState state;
		_shapeEditor->get_EditorState(&state);
		if (state != EditorCreationUnbound)
		{
			VARIANT_BOOL vb;
			ShpfileType shpType = _shapeEditor->GetShapeTypeForTool((tkCursorMode)m_cursorMode);
			_shapeEditor->StartUnboundShape(shpType, &vb);
			_shapeEditor->ApplyColoringForTool((tkCursorMode)m_cursorMode);
		}
		HandleOnLButtonShapeAddMode(x, y, projX, projY, ctrl);
		return;
	}

	// --------------------------------------------
	//  Handling particular cursor modes
	// --------------------------------------------
	switch(m_cursorMode)
	{
		case cmRotateShapes:
		case cmMoveShapes:
			HandleOnLButtonMoveOrRotate(x, y);
			break;
		case cmAddShape:
			HandleOnLButtonShapeAddMode(x, y, projX, projY, ctrl);
			break;
		case cmEditShape:
			HandleOnLButtonDownShapeEditor(x, y, ctrl);
			break;
		case cmZoomIn:
			{
				this->SetCapture();
				_dragging.Operation = DragZoombox;
			}
			break;
		case cmZoomOut:
			ZoomOut( m_zoomPercent );
			break;
		case cmPan:
			{
				CRect rcBounds(0,0,_viewWidth,_viewHeight);
				this->LogPrevExtent();
				this->SetCapture();
				_dragging.Operation = DragPanning;
			}
			break;
		case cmSelection:
			HandleLButtonDownSelection(point, vbflags);
			break;
		case cmMeasure:
			{
				bool added = true;
				if (snapping){
					GetMeasuringBase()->HandleProjPointAdd(projX, projY);
				}
				else {
					added = GetMeasuringBase()->HandlePointAdd(x, y, ctrl);
				}

				if (added)
				{
					FireMeasuringChanged(_measuring, tkMeasuringAction::PointAdded);
					if( m_sendMouseDown ) this->FireMouseDown( MK_LBUTTON, (short)vbflags, x, y );
					RedrawCore(RedrawSkipDataLayers, false, true);
				}
			}
			break;
		
	default: //if( m_cursorMode == cmNone )
		{
			SetCapture();
			if( m_sendMouseDown == TRUE )
				this->FireMouseDown(MK_LBUTTON, (short)vbflags, x, y);
		}
		break;
	}
}

// ************************************************************
//		OnLButtonDblClick
// ************************************************************
void CMapView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	ZoombarPart part = ZoombarHitTest(point.x, point.y);
	if (part != ZoombarPart::ZoombarNone)
		return;

	OnLButtonDown(nFlags, point);
	if (m_cursorMode == cmMeasure)
	{
		_measuring->FinishMeasuring();
		FireMeasuringChanged(_measuring, tkMeasuringAction::MesuringStopped);	
		return;
	}
	
	// add a vertex							
	if (m_cursorMode == cmEditShape) 
	{
		double projX, projY;
		PixelToProj(point.x, point.y, &projX, &projY);
		if (_shapeEditor->GetClosestPoint(projX, projY, projX, projY))
		{
			if (_shapeEditor->InsertVertex(projX, projY)) {
				RedrawCore(tkRedrawType::RedrawSkipDataLayers, false, true);
				return;
			}
		}
	}
}

// ************************************************************
//		OnLButtonUp
// ************************************************************
void CMapView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (HasRotation())
		_rotate->getOriginalPixelPoint(point.x, point.y, &(point.x), &(point.y));
	
	long vbflags = ParseKeyboardEventFlags(nFlags);

	if( m_sendMouseUp == TRUE && _leftButtonDown )
		FireMouseUp( MK_LBUTTON, (short)vbflags, point.x, point.y - 1 );

	DraggingOperation operation = _dragging.Operation;

	_leftButtonDown = FALSE;
	
	ReleaseCapture();

	switch(operation)
	{
		case DragRotateShapes:
			{
				IShapefile* sf = _dragging.Shapefile;
				if (sf)
				{
					double angle = GetDraggingRotationAngle();
					ShapefileHelper::Rotate(sf, _dragging.RotateCenter.x, _dragging.RotateCenter.y, angle);
					RegisterGroupOperation(operation);
					Redraw();
				}
			}
			break;
		case DragMoveShapes:
			{
				IShapefile* sf = _dragging.Shapefile;
				if (sf) 
				{
					Point2D pnt = GetDraggingProjOffset();
					VARIANT_BOOL vb;
					sf->Move(pnt.x, pnt.y, &vb);
					RegisterGroupOperation(operation);
					Redraw();
				}
			}
			break;
		case DragMoveVertex:
		case DragMovePart:
		case DragMoveShape:
			{
				if (HandleLButtonUpDragVertexOrShape(nFlags))
					Redraw2(tkRedrawType::RedrawSkipDataLayers);
			}
			break;
		case DragPanning:
			if (m_cursorMode != cmPan)
				Debug::WriteError("Wrong cursor mode when panning is expected");

			ReleaseCapture();

			if (!_spacePressed)
				DisplayPanningInertia(point);

			this->SetExtentsCore(this->_extents, false);

			ClearPanningList();

			// we need to redraw the layers
			_dragging.Operation = DragNone;		// don't clear dragging state until the end of animation; it won't offset the layers
			Redraw2(tkRedrawType::RedrawAll);
			break;
		case DragZoombarHandle:
			if (_zoombarTargetZoom == -1)
				Debug::WriteError("Invalid target zoom for zoom bar");

			ZoomToTileLevel(_zoombarTargetZoom);
			_dragging.Operation = DragNone;
			break;
		case DragZoombox:
			_dragging.Operation = DragNone;
			if (!_dragging.HasRectangle())
			{
				ZoomIn( m_zoomPercent );
			}
			else
			{
				CRect rect = _dragging.GetRectangle();

				if (HasRotation())	// TODO: wrap in class
				{
					CRect rectTmp = rect;
					long tmpX = 0, tmpY = 0;
					_rotate->getOriginalPixelPoint(rect.left, rect.top, &tmpX, &tmpY);
					rectTmp.TopLeft().x = tmpX;
					rectTmp.TopLeft().y = tmpY;
					_rotate->getOriginalPixelPoint(rect.right, rect.bottom, &tmpX, &tmpY);
					rectTmp.BottomRight().x = tmpX;
					rectTmp.BottomRight().y = tmpY;
					rect = rectTmp;
				}
				
				double rx, by, lx, ty;
				PixelToProjection( rect.TopLeft().x, rect.TopLeft().y, rx, by );
				PixelToProjection( rect.BottomRight().x, rect.BottomRight().y, lx, ty );

				double cLeft = MINIMUM( rx, lx );
				double cRight = MAXIMUM( rx, lx );
				double cBottom = MINIMUM( ty, by );
				double cTop = MAXIMUM( ty, by );

				SetNewExtentsWithForcedZooming(Extent(cLeft, cRight, cBottom, cTop), true);

				if( m_sendSelectBoxFinal )
				{
					long iby = rect.BottomRight().y;
					long ity = rect.TopLeft().y;
					this->FireSelectBoxFinal( rect.TopLeft().x, rect.BottomRight().x, iby, ity );
				}
			}
			break;
	}
	_dragging.Clear();
	Utility::ClosePointer(&_moveBitmap);
}

// ************************************************************
//		DisplayPanningInertia
// ************************************************************
void CMapView::DisplayPanningInertia( CPoint point )
{
	if (HasDrawingData(PanningInertia))
	{
		if (_panningInertia == csFalse) return;

		bool inertia = false;
		double dx = 0.0, dy = 0.0;
		DWORD normalInterval = Utility::Rint(0.3 * CLOCKS_PER_SEC);		// normal interval
		DWORD interval = 0;												// measured interval
		DWORD timeNow = GetTickCount();

		// -----------------------------------------------------------
		//	 Choosing the interval for speed calculation
		// -----------------------------------------------------------
		_panningLock.Lock();
		size_t size = _panningList.size();

		if (size > 1 )
		{
			DWORD minTime = timeNow - normalInterval;
			int firstIndex = size - 2;

			for(int i = size - 1; i >= 0; i--)
			{
				if (_panningList[i]->time < minTime)
				{
					firstIndex = i;
					break;
				}
			}

			if (firstIndex != -1)
			{
				dx = _panningList[size - 1]->x - _panningList[firstIndex]->x;
				dy = _panningList[size - 1]->y - _panningList[firstIndex]->y;
				interval = timeNow - _panningList[firstIndex]->time;
				inertia = true;
			}
		}
		_panningLock.Unlock();

		// -----------------------------------------------------------
		//	 Rendering inertia
		// -----------------------------------------------------------
		if (inertia)
		{
			// for small map the same inertia is perceived as being faster
			double coeff = 1.5 + (_viewWidth * _viewHeight) / 1e6 * 0.7;	
			double ratio = normalInterval / (double)interval * coeff;		
			dx *= ratio;
			dy *= ratio;

			double dist = sqrt(pow(dx, 2.0) + pow(dy, 2.0));

			int numSteps = 0, sum = 0;
			do 
			{
				numSteps++;
				sum += (numSteps * 3);
			} while (sum < dist);

			if (numSteps > 1)
			{
				_panningAnimation = true;
				for (int i = numSteps; i >= 0; i--) 
				{
					DWORD lastTime = GetTickCount();
					_dragging.Move.x += Utility::Rint(dx * i * 3 / sum);
					_dragging.Move.y += Utility::Rint(dy * i * 3 / sum);
					DoPanning(_dragging.Move);
					Debug::WriteWithTime("Drawing inertia");

					// if rendering is too fast, let's introduce some artificial slowness
					DWORD timeNow = GetTickCount();
					if ( timeNow - lastTime < 80 ) {
						Sleep( 80 - timeNow + lastTime);
					}

					MSG msg;
					// remove all key down, so that pressed TAB won't be processed after the end of animation
					if (::PeekMessage(&msg, NULL, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE )) 
						break;

					// let user stop animation with left button click
					if (::PeekMessage(&msg, NULL, WM_LBUTTONDOWN, WM_LBUTTONDOWN, PM_NOREMOVE ))
						break;
				}
				_panningAnimation = false;
			}
		}
	}
}
#pragma endregion

#pragma region Mouse move

// ************************************************************
//		ShowToolTipOnMouseMove
// ************************************************************
void CMapView::ShowToolTipOnMouseMove(UINT nFlags, CPoint point)
{
	if (_showingToolTip)
	{
		CToolInfo cti;
		_ttip.GetToolInfo(cti, this, IDC_TTBTN);
		cti.rect.left = point.x - 2;
		cti.rect.right = point.x + 2;
		cti.rect.top = point.y - 2;
		cti.rect.bottom = point.y + 2;
		_ttip.SetToolInfo(&cti);
		_ttip.SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

		MSG pMsg;
		pMsg.hwnd = this->m_hWnd;
		pMsg.message = WM_MOUSEMOVE;
		pMsg.wParam = nFlags;
		pMsg.lParam = MAKELPARAM(point.x, point.y);
		pMsg.pt = point;
		_ttip.RelayEvent(&pMsg);
	}
}

// ************************************************************
//		OnMouseMove
// ************************************************************
void CMapView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (HasRotation())
		_rotate->getOriginalPixelPoint(point.x, point.y, &(point.x), &(point.y));
	_dragging.Move = point;

	if (HandleOnZoombarMouseMove(point))
		return;

	ShowToolTipOnMouseMove(nFlags, point);

	long mbutton = ParseMouseEventFlags(nFlags);
	long vbflags = ParseKeyboardEventFlags(nFlags);
	
	if( m_sendMouseMove == TRUE )
	{	
		if( (m_cursorMode == cmPan) && (nFlags & MK_LBUTTON ))
		{
			//Do Not Send the Event
		}
		else
		{
			this->FireMouseMove( (short)mbutton, (short)vbflags, point.x, point.y );
		}
	}

	bool updateHotTracking = true;
	bool refreshNeeded = false;

	if (IsDigitizingCursor() || m_cursorMode == cmMeasure)
	{
		ActiveShape* shp = GetActiveShape();
		if (shp->IsDynamic() && shp->GetPointCount() > 0)
		{
			VARIANT_BOOL snapped = VARIANT_FALSE;
			double x = point.x, y = point.y;
			tkSnapBehavior behavior;
			if (SnappingIsOn(nFlags, behavior) && behavior == sbSnapByDefault)
			{
				snapped = this->FindSnapPoint(GetMouseTolerance(ToleranceSnap, false), point.x, point.y, &x, &y);
				if (snapped) {
					ProjToPixel(x, y, &x, &y);
				}
			}
			shp->SetMousePosition(x, y);
			refreshNeeded = true;
		}
	}
	
	switch(m_cursorMode)
	{
		case cmMoveShapes:
		case cmRotateShapes:
			if (_dragging.Operation == DragMoveShapes ||
				_dragging.Operation == DragRotateShapes)
			{
				if (InitDraggingShapefile())
				{
					this->Refresh();
					return;
				}
				refreshNeeded = true;
			}
			break;
		case cmZoomIn:
			if ((nFlags & MK_LBUTTON) && _leftButtonDown) {
				refreshNeeded = true;
				updateHotTracking = false;
			}
			break;
		case cmPan:
			if( (nFlags & MK_LBUTTON) && _leftButtonDown )
			{
				DWORD time = GetTickCount();
				if (_panningInertia != csFalse)
				{
					_panningLock.Lock();
					_panningList.push_back(new TimedPoint(point.x, point.y, time));
					_panningLock.Unlock();
				}
				DoPanning(point);
				return;
			}
			break;
		case cmEditShape:
		{
			if (HandleOnMouseMoveShapeEditor(point.x, point.y, nFlags))
				refreshNeeded = true;
		}
		break;
	}

	if (updateHotTracking) 
	{
		if (UpdateHotTracking(point))
			refreshNeeded = true;
	}

	if (_showCoordinates != cdmNone) {
		refreshNeeded = true;
	}

	if (refreshNeeded) {
		this->Refresh();
	}
}

// ************************************************************
//		DoPanning
// ************************************************************
void CMapView::DoPanning(CPoint point)
{
	double xAmount = (_dragging.Start.x - _dragging.Move.x) * _inversePixelPerProjectionX;
	double yAmount = (_dragging.Move.y - _dragging.Start.y) * _inversePixelPerProjectionY;

	Debug::WriteWithTime("Panning amount: x=%d; y=%d", _dragging.Start.x - _dragging.Move.x, _dragging.Move.y - _dragging.Start.y);
	Debug::WriteWithTime("Clicked down extents: %f %f %f %f", _clickDownExtents.left, _clickDownExtents.right, _clickDownExtents.bottom, _clickDownExtents.top);

	_extents.left = _clickDownExtents.left + xAmount;
	_extents.right = _clickDownExtents.right + xAmount;
	_extents.bottom = _clickDownExtents.bottom + yAmount;
	_extents.top = _clickDownExtents.top + yAmount;
	
	// trigger redraw of scalebar; commented: doesn't look nice + additional computational burden
	//m_lastWidthMeters = 0.0;	

	if (_useSeamlessPan)
	{
		// complete redraw; bad for performance, especially for large layers
		// AxMap.LayerScreenBufferMode property should be used instead to mark
		// layer for immediate redraw
		_canUseLayerBuffer = FALSE;
		LockWindow(lmUnlock);	
		FireExtentsChanged(); 
		ReloadImageBuffers(); 
	}	
	else
	{
		// layers stay the same, while all the rest must be updated
		RedrawCore(RedrawSkipDataLayers, true, true);
	}
}
#pragma endregion

#pragma region Right and middle buttons
// ************************************************************
//		OnRButtonDblClick
// ************************************************************
void CMapView::OnRButtonDblClk(UINT nFlags, CPoint point)
{
   OnRButtonDown(nFlags, point);
}

// ************************************************************
//		OnRButtonDown
// ************************************************************
void CMapView::OnRButtonDown(UINT nFlags, CPoint point)
{
	ZoombarPart part = ZoombarHitTest(point.x, point.y);
	if (part != ZoombarPart::ZoombarNone)
		return;

	long vbflags = ParseKeyboardEventFlags(nFlags);

	if( m_sendMouseDown == TRUE )
		this->FireMouseDown( MK_RBUTTON, (short)vbflags, point.x, point.y - 1 );

	if (_doTrapRMouseDown == TRUE)
	{
		VARIANT_BOOL redraw;
		if( m_cursorMode == cmMeasure)
		{
			((CMeasuring*)_measuring)->UndoPoint(&redraw);
			FireMeasuringChanged(_measuring, tkMeasuringAction::PointRemoved);
			_canUseMainBuffer = false;
		}
		else if (IsDigitizingCursor())
		{
			_shapeEditor->Undo(&redraw);
			_canUseMainBuffer = false;
		}

		_reverseZooming = true;

		if( m_cursorMode == cmZoomOut )
		{
			double zx = _extents.left, zy = _extents.bottom;
			PixelToProjection( point.x, point.y, zx, zy );

			double halfxRange = (_extents.right - _extents.left)*.5;
			double halfyRange = (_extents.top - _extents.bottom)*.5;

			_extents.left = zx - halfxRange;
			_extents.right = zx + halfxRange;
			_extents.bottom = zy - halfyRange;
			_extents.top = zy + halfyRange;

			ZoomIn( m_zoomPercent );
			::SetCursor( _cursorZoomin );

			FireExtentsChanged();
			ReloadImageBuffers();
		}
		else if( m_cursorMode == cmZoomIn )
		{
			double zx = _extents.left, zy = _extents.bottom;
			PixelToProjection( point.x, point.y, zx, zy );

			double halfxRange = (_extents.right - _extents.left)*.5;
			double halfyRange = (_extents.top - _extents.bottom)*.5;

			_extents.left = zx - halfxRange;
			_extents.right = zx + halfxRange;
			_extents.bottom = zy - halfyRange;
			_extents.top = zy + halfyRange;

			ZoomOut( m_zoomPercent );
			::SetCursor( _cursorZoomout );

			FireExtentsChanged();
			ReloadImageBuffers();
		}

		if( redraw ) {
			this->Refresh();
		}
	}
}

// *********************************************************
//		OnRButtonUp()
// *********************************************************
void CMapView::OnRButtonUp(UINT nFlags, CPoint point)
{
	COleControl::OnRButtonUp(nFlags, point);
	ReleaseCapture();//why is this being called, capture isn't set on RButtonDown as far as I can see...

	_reverseZooming = false;
	if (m_cursorMode == cmZoomIn) ::SetCursor( _cursorZoomin );
	if (m_cursorMode == cmZoomOut) ::SetCursor( _cursorZoomout );

	long vbflags = ParseKeyboardEventFlags(nFlags);

	if( m_sendMouseUp == TRUE )
		this->FireMouseUp( MK_RBUTTON, (short)vbflags, point.x, point.y - 1 );
}

// *********************************************************
//		OnMButtonUp()
// *********************************************************
void CMapView::OnMButtonUp(UINT nFlags, CPoint point)
{
	double zx = _extents.left, zy = _extents.bottom;
	PixelToProjection( point.x, point.y, zx, zy );

	double halfxRange = (_extents.right - _extents.left)*.5;
	double halfyRange = (_extents.top - _extents.bottom)*.5;

	_extents.left = zx - halfxRange;
	_extents.right = zx + halfxRange;
	_extents.bottom = zy - halfyRange;
	_extents.top = zy + halfyRange;

	this->SetExtentsCore(_extents);
}
#pragma endregion

#pragma region Others
// *************************************************************
//		CMapView::OnSize()
// *************************************************************
void CMapView::OnSize(UINT nType, int cx, int cy)
{
	// the redraw is prohibited before the job here is done
	_isSizing = true;

	COleControl::OnSize(nType, cx, cy);

	CDC* pDC = GetDC();
	
	ResizeBuffers(cx, cy);
	
	// we shall fill the new regions with back color
	if (cx > _viewWidth)
	{
		pDC->FillSolidRect(_viewWidth, 0, cx - _viewWidth, cy, m_backColor);
	}
	if (cy > _viewHeight)
	{
		pDC->FillSolidRect(0, _viewHeight, cx, cy - _viewHeight, m_backColor);
	}

	ReleaseDC(pDC);

	if( cx > 0 && cy > 0 )
	{
		_viewWidth = cx;
		_viewHeight = cy;
		_aspectRatio = (double)_viewWidth/(double)_viewHeight;
		_isSizing = false;
		
		this->SetExtentsCore(_extents, false, true);
	}
	else
	{
		_isSizing = false;
	}
}

// *******************************************************
//		OnDropFiles()
// *******************************************************
void CMapView::OnDropFiles(HDROP hDropInfo)
{
	long numFiles = DragQueryFile( hDropInfo, 0xFFFFFFFF, NULL, 0 );

	register int i;
	for( i = 0; i < numFiles; i++ )
	{
		long fsize = DragQueryFile( hDropInfo, i, NULL, 0 );
		if( fsize > 0 )
		{	char * fname = new char[fsize + 2];
			DragQueryFile( hDropInfo, i, fname, fsize + 1 );
			FireFileDropped( fname );
			delete [] fname;
		}
	}
	COleControl::OnDropFiles(hDropInfo);
}

// *******************************************************
//		OnBackColorChanged()
// *******************************************************
void CMapView::OnBackColorChanged()
{
	_canUseLayerBuffer = FALSE;
	if( !_lockCount )
		InvalidateControl();
}


// *********************************************************
//		OnResetState 
// *********************************************************
// Reset control to default state
void CMapView::OnResetState()
{
	COleControl::OnResetState();  // Resets defaults found in DoPropExchange
	// TODO: Reset any other control state here.
}

// *********************************************************
//		Unimplemented events
// *********************************************************
void CMapView::OnExtentPadChanged(){}
void CMapView::OnExtentHistoryChanged(){}
void CMapView::OnKeyChanged(){}
void CMapView::OnDoubleBufferChanged(){}
void CMapView::OnZoomPercentChanged(){}
void CMapView::OnUDCursorHandleChanged(){}
void CMapView::OnSendMouseDownChanged(){}
void CMapView::OnSendOnDrawBackBufferChanged(){}
void CMapView::OnSendMouseUpChanged(){}
void CMapView::OnSendMouseMoveChanged(){}
void CMapView::OnSendSelectBoxDragChanged(){}
void CMapView::OnSendSelectBoxFinalChanged(){}
#pragma endregion

// *************************************************************
//		OnTimer()
// *************************************************************
#ifdef WIN64
void CMapView::OnTimer(UINT_PTR nIDEvent)
#else
void CMapView::OnTimer(UINT nIDEvent)
#endif
{
	// TODO: Add your message handler code here and/or call default
	if( nIDEvent == SHOWTEXT )
	{	KillTimer(SHOWTEXT);
	_showingToolTip = TRUE;
	}
	else if( nIDEvent == HIDETEXT)
	{	KillTimer(HIDETEXT);
	_showingToolTip = FALSE;

	CToolInfo cti;
	_ttip.GetToolInfo(cti,this,IDC_TTBTN);
	cti.rect.left = -1;
	cti.rect.top = - 1;
	cti.rect.right = - 1;
	cti.rect.bottom = - 1;
	_ttip.SetToolInfo(&cti);
	_ttip.Pop();
	}

	COleControl::OnTimer(nIDEvent);
}
