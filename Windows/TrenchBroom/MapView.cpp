/*
 Copyright (C) 2010-2012 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "TrenchBroom.h"
#endif

#include <cassert>
#include "MapDocument.h"
#include "MapView.h"
#include "WinStringFactory.h"
#include "WinUtilities.h"

#include "Controller/Editor.h"
#include "Gwen/Controls/Canvas.h"
#include "GUI/EditorGui.h"
#include "IO/FileManager.h"
#include "Model/Map/Map.h"
#include "Model/Map/EntityDefinition.h"
#include "Renderer/FontManager.h"
#include "Utilities/Utils.h"
#include "Utilities/Console.h"

#include <string>
#include <cassert>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMapView

IMPLEMENT_DYNCREATE(CMapView, CView)

BEGIN_MESSAGE_MAP(CMapView, CView)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_KEYDOWN()
	ON_WM_CHAR()
	ON_WM_KEYUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_MOUSEHWHEEL()
END_MESSAGE_MAP()

// CMapView construction/destruction

CMapView::CMapView() : m_lastMousePos(NULL), m_deviceContext(NULL), m_openGLContext(NULL), m_editorGui(NULL), m_fontManager(NULL)
{
	// TODO: add construction code here
	for (unsigned int i = 0; i < 3; i++)
		m_mouseButtonDown[i] = FALSE;
}

CMapView::~CMapView()
{
	if (m_lastMousePos != NULL)
		delete m_lastMousePos;
}

BOOL CMapView::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.style |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	return CView::PreCreateWindow(cs);
}

// CMapView drawing

void CMapView::OnDraw(CDC* /*pDC*/)
{
	if (!wglMakeCurrent(m_deviceContext, m_openGLContext))
		return;

	RECT clientRect;
	GetClientRect(&clientRect);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(clientRect.left, clientRect.right, clientRect.bottom, clientRect.top, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0, 0, clientRect.right, clientRect.bottom);

	m_editorGui->resizeTo(clientRect.right, clientRect.bottom);
	m_editorGui->render();

	glFlush();
	SwapBuffers(m_deviceContext);
}


// CMapView diagnostics

#ifdef _DEBUG
void CMapView::AssertValid() const
{
	CView::AssertValid();
}

void CMapView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CMapDocument* CMapView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMapDocument)));
	return (CMapDocument*)m_pDocument;
}
#endif //_DEBUG

bool CMapView::mapViewFocused()
{
	return m_editorGui->mapViewFocused();
}


// CMapView message handlers


int CMapView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_deviceContext = GetDC()->m_hDC;

	PIXELFORMATDESCRIPTOR descriptor = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,								// version
		PFD_DRAW_TO_WINDOW |			//draw to window
		PFD_SUPPORT_OPENGL |			// use OpenGL
		PFD_DOUBLEBUFFER,				// use double buffering
		PFD_TYPE_RGBA,					// use RGBA pixels
		32,								// 32 bit color depth
		0, 0, 0,						// RGB bits and shifts
		0, 0, 0,						// ...
		0, 0,							// alpha buffer info
		0, 0, 0, 0, 0,					// accumulation buffer
		32,								// depth buffer
		8,								// stencil buffer
		0,								// aux buffers
		PFD_MAIN_PLANE,					// layer type
		0,								// reserved (must be 0)
		0,								// no layer mask
		0,								// no visible mask
		0								// no damage mask
	};

	GLint iAttributes[] = {
		WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_ACCELERATION_ARB,	WGL_FULL_ACCELERATION_ARB,
        WGL_COLOR_BITS_ARB,		24,
        WGL_ALPHA_BITS_ARB,		8,
        WGL_DEPTH_BITS_ARB,		32,
        WGL_STENCIL_BITS_ARB,	8,
        WGL_DOUBLE_BUFFER_ARB,	GL_TRUE,
        WGL_SAMPLE_BUFFERS_ARB,	GL_TRUE,
        WGL_SAMPLES_ARB,		4,
        0,0};
	int tryDepthAttributes[] = {32, 24,16};
	int trySampleAttributes[] = {4, 2};
	
	GLfloat fAttributes[] = {0,0};
	GLint pixelFormat = -1;
	GLuint numFormats = 0;

	CFrameWnd* testWindow = new CFrameWnd();
	if (!testWindow->Create(NULL, "Test Window"))
		return -1;

	HDC testDC = testWindow->GetDC()->m_hDC;
	pixelFormat = ChoosePixelFormat(testDC, &descriptor);
	SetPixelFormat(testDC, pixelFormat, &descriptor);
	HGLRC testGLContext = wglCreateContext(testDC);
	wglMakeCurrent(testDC, testGLContext);

	bool valid = true;
	for (unsigned int i = 0; i < 3 && valid && numFormats == 0; i++) {
		iAttributes[11] = tryDepthAttributes[i];
		for (unsigned int j = 0; j < 2 && valid && numFormats == 0; j++) {
			iAttributes[19] = trySampleAttributes[j];
			valid = wglChoosePixelFormatARB(testDC, iAttributes, fAttributes, 1, &pixelFormat, &numFormats) == TRUE;
		}
	}

	wglDeleteContext(testGLContext);
	testGLContext = NULL;
	testWindow->DestroyWindow();
	testWindow = NULL;
	testDC = NULL;

	if (!valid || numFormats == 0) {
		TrenchBroom::log(TrenchBroom::TB_LL_INFO, "Multisampling disabled\n");
		pixelFormat = ChoosePixelFormat(m_deviceContext, &descriptor);
	}

	SetPixelFormat(m_deviceContext, pixelFormat, &descriptor);
	m_openGLContext = wglCreateContext(m_deviceContext);
	wglMakeCurrent(m_deviceContext, m_openGLContext);
	if (!wglSwapIntervalEXT(1) || wglGetSwapIntervalEXT() == 0)
		TrenchBroom::log(TrenchBroom::TB_LL_INFO, "Vertical sync disabled\n");

	char appPath [MAX_PATH] = "";

	::GetModuleFileName(0, appPath, sizeof(appPath) - 1);
	TrenchBroom::IO::FileManager& fileManager = *TrenchBroom::IO::FileManager::sharedFileManager;
	std::string appDirectory = fileManager.deleteLastPathComponent(appPath);
	std::string resDirectory = fileManager.appendPath(appDirectory, "Resources");
	std::string skinPath = fileManager.appendPath(resDirectory, "DefaultSkin.png");
	if (!fileManager.exists(skinPath)) {
		wglDeleteContext(m_openGLContext);
		m_openGLContext = NULL;
		return -1;
	}

	TrenchBroom::Renderer::StringFactory* stringFactory = new TrenchBroom::Renderer::WinStringFactory(GetDC()->m_hDC);
	m_fontManager = new TrenchBroom::Renderer::FontManager(stringFactory);
	m_editorGui = new TrenchBroom::Gui::EditorGui(GetDocument()->editor(), *m_fontManager, skinPath);
	m_editorGui->editorGuiRedraw += new TrenchBroom::Gui::EditorGui::EditorGuiEvent::Listener<CMapView>(this, &CMapView::editorGuiRedraw);

	return 0;
}

void CMapView::editorGuiRedraw(TrenchBroom::Gui::EditorGui& editorGui)
{
	Invalidate(FALSE);
}

void CMapView::OnDestroy()
{
	wglMakeCurrent(m_deviceContext, m_openGLContext);

	CView::OnDestroy();

	m_editorGui->editorGuiRedraw -= new TrenchBroom::Gui::EditorGui::EditorGuiEvent::Listener<CMapView>(this, &CMapView::editorGuiRedraw);
	delete m_editorGui;
	delete m_fontManager;

	wglDeleteContext(m_openGLContext);
}


void CMapView::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_mouseButtonDown[0] = TRUE;
	OnMouseMove(nFlags, point);
	SetCapture();
	m_editorGui->canvas()->InputMouseButton(0, true);	
}


void CMapView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (!m_mouseButtonDown[0])
		return;

	OnMouseMove(nFlags, point);
	m_editorGui->canvas()->InputMouseButton(0, false);
	ReleaseCapture();
	m_mouseButtonDown[0] = FALSE;
}


void CMapView::OnRButtonDown(UINT nFlags, CPoint point)
{
	m_mouseButtonDown[1] = TRUE;
	OnMouseMove(nFlags, point);
	SetCapture();
	m_editorGui->canvas()->InputMouseButton(1, true);	
}


void CMapView::OnRButtonUp(UINT nFlags, CPoint point)
{
	if (!m_mouseButtonDown[1])
		return;

	OnMouseMove(nFlags, point);
	if (!m_editorGui->canvas()->InputMouseButton(1, false) && m_editorGui->mapViewHovered()) {
		CMenu parentMenu;
		BOOL success = parentMenu.LoadMenuA(IDR_MAPVIEW_POPUP);
		assert(success);

		CMenu* popupMenu = parentMenu.GetSubMenu(0);
		assert(popupMenu != NULL);

		CPoint screen = point;
		ClientToScreen(&screen);
		popupMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, screen.x, screen.y, GetTopLevelParent(), NULL);
	}
	ReleaseCapture();
	m_mouseButtonDown[1] = FALSE;
}


BOOL CMapView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	m_editorGui->canvas()->InputMouseWheel(zDelta / 10.0f);
	return TRUE;
}

void CMapView::OnMouseHWheel(UINT nFlags, short zDelta, CPoint pt)
{
	OnMouseWheel(nFlags, zDelta, pt);
}

void CMapView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_lastMousePos == NULL)
		m_lastMousePos = new CPoint(point);
	else if (m_lastMousePos->x == point.x && m_lastMousePos->y == point.y)
		return;

	m_editorGui->canvas()->InputMouseMoved(point.x, point.y, point.x - m_lastMousePos->x, point.y - m_lastMousePos->y);
	*m_lastMousePos = point;
}


void CMapView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	key(nChar, nFlags, true);
}

void CMapView::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	m_editorGui->canvas()->InputCharacter(Gwen::UnicodeChar(nChar));
}


void CMapView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	key(nChar, nFlags, false);
}


void CMapView::key(UINT nChar, UINT nFlags, bool down)
{
	int iKey = gwenKey(nChar, nFlags);
	if ( iKey != -1 )
		m_editorGui->canvas()->InputKey(iKey, down);
}

int CMapView::gwenKey(UINT nChar, UINT nFlags)
{
	if ( nChar == VK_SHIFT )	return Gwen::Key::Shift;
	if ( nChar == VK_RETURN )	return Gwen::Key::Return;
	if ( nChar == VK_BACK )		return Gwen::Key::Backspace;
	if ( nChar == VK_DELETE )	return Gwen::Key::Delete;
	if ( nChar == VK_LEFT )		return Gwen::Key::Left;
	if ( nChar == VK_RIGHT )	return Gwen::Key::Right;
	if ( nChar == VK_TAB )		return Gwen::Key::Tab;
	if ( nChar == VK_HOME )		return Gwen::Key::Home;
	if ( nChar == VK_END )		return Gwen::Key::End;
	if ( nChar == VK_CONTROL )	return Gwen::Key::Control;
	if ( nChar == VK_MENU )		return Gwen::Key::Alt;
	if ( nChar == VK_UP )		return Gwen::Key::Up;
	if ( nChar == VK_DOWN )		return Gwen::Key::Down;
	return -1;

}
